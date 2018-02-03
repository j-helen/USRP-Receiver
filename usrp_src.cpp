#include "usrp_src.h"

using namespace std;
namespace po = boost::program_options;
bool stop_signal_called = false;
void sig_int_handler(int){stop_signal_called = true;}

uhd::time_spec_t _start_time;

bool check_locked_sensor(std::vector<std::string> sensor_names, const char* sensor_name, get_sensor_fn_t get_sensor_fn, double setup_time){
    if (std::find(sensor_names.begin(), sensor_names.end(), sensor_name) == sensor_names.end())
        return false;

    boost::system_time start = boost::get_system_time();
    boost::system_time first_lock_time;

    std::cout << boost::format("Waiting for \"%s\": ") % sensor_name;
    std::cout.flush();

    while (true) {
        if ((not first_lock_time.is_not_a_date_time()) and
                (boost::get_system_time() > (first_lock_time + boost::posix_time::seconds(setup_time))))
        {
            std::cout << " locked." << std::endl;
            break;
        }
        if (get_sensor_fn(sensor_name).to_bool()){
            if (first_lock_time.is_not_a_date_time())
                first_lock_time = boost::get_system_time();
            std::cout << "+";
            std::cout.flush();
        }
        else {
            first_lock_time = boost::system_time();	//reset to 'not a date time'

            if (boost::get_system_time() > (start + boost::posix_time::seconds(setup_time))){
                std::cout << std::endl;
                throw std::runtime_error(str(boost::format("timed out waiting for consecutive locks on sensor \"%s\"") % sensor_name));
            }
            std::cout << "_";
            std::cout.flush();
        }
        boost::this_thread::sleep(boost::posix_time::milliseconds(100));
    }
    std::cout << std::endl;
    return true;
}

unsigned int usrp_src::init(double rate, double freq, double gain, char* hostfmt, char* wirefmt)
{
	uhd::set_thread_priority_safe();
	unsigned int samples_per_recv=0;
	bool null=false;

	const char *args = "";
	uhd::device_addr_t dev_addr(args);
	//create a usrp device
    std::cout << std::endl;
    std::cout << boost::format("Creating the usrp device with: %s") % args << std::endl;
    usrp = uhd::usrp::multi_usrp::make(dev_addr);
	if(!usrp){
		std::cout << "Unable to find USRP device. Unable to issue streaming command.\n" << std::endl;
		return ~0;
	}
//Set up clock reference. Lock mboard clocks
	usrp->set_clock_source("internal");

	std::cout << boost::format("Using Device: %s") % usrp->get_pp_string() << std::endl;

//set the sample rate
    if (rate <= 0.0){
        std::cout << "Please specify a valid sample rate" << std::endl;
        return ~0;
    }
    std::cout << boost::format("Setting RX Rate: %f Msps...") % (rate/1e6) << std::endl;
    usrp->set_rx_rate(rate);
    std::cout << boost::format("Actual RX Rate: %f Msps...") % (usrp->get_rx_rate()/1e6) <<     std::endl << std::endl;
	std::cout<< boost::format("Host must support a rate of atleast %f MB/s.\n") % (usrp->get_rx_rate() * 2 * sizeof(float) / 1e6) << std::endl;


//set the center frequency
    if (freq) { //with default of 0.0 this will always be true
        std::cout << boost::format("Setting RX Freq: %f MHz...") % (freq/1e6) << std::endl;
		uhd::tune_request_t tune_request(freq,10e6);
		tune_request.args = uhd::device_addr_t("mode_n=integer");
        usrp->set_rx_freq(tune_request,0);
       std::cout << boost::format("Actual RX Freq: %f MHz...") % (usrp->get_rx_freq()/1e6) << std::endl << std::endl;
    }

//Setting DC offset
	usrp->set_rx_dc_offset(true);

 //set the rf gain
    if (gain) {
        std::cout << boost::format("Setting RX Gain: %f dB...") % gain << std::endl;
        usrp->set_rx_gain(gain);
        std::cout << boost::format("Actual RX Gain: %f dB...") % usrp->get_rx_gain() << std::endl << std::endl;
	}


//set the analog bandwidth
/*	if(bw){
		std::cout << boost::format("Setting RX Bandwidth: %f MHz...") % (bw/1e6) << std::endl;
		usrp->set_rx_bandwidth(bw);
		std::cout << boost::format("Actual RX Bandwidth: %f MHz...") % (usrp->get_rx_bandwidth()/1e6) << std::endl << std::endl;
    }*/

/* create rx streamer */
		uhd::stream_args_t stream_args(hostfmt,wirefmt); //argument
		if (stream_args.cpu_format == "fc32") _type = boost::make_shared<uhd::io_type_t>(uhd::io_type_t::COMPLEX_FLOAT32);
		if (stream_args.cpu_format == "sc16") _type = boost::make_shared<uhd::io_type_t>(uhd::io_type_t::COMPLEX_INT16);
		//uhd::stream_args_t stream_args("sc16");
		rx_stream = usrp->get_rx_stream(stream_args); //receiver arguments

check_locked_sensor(usrp->get_rx_sensor_names(0), "lo_locked", boost::bind(&uhd::usrp::multi_usrp::get_rx_sensor, usrp, _1, 0), 1.0);

boost::this_thread::sleep(boost::posix_time::seconds(1)); //allow for some setup time

        std::signal(SIGINT, &sig_int_handler);
        std::cout << "Press Ctrl + C to stop streaming..." << std::endl;
}
float usrp_src::stream_fl(unsigned int samples_per_recv, std::complex<float>* buffs)
{
	size_t num_rx_samps=0;
	size_t num_acc_samps=0;
	unsigned long long num_total_samps = 0;
	double time_requested = 3.0; // total time to receive, can also be inputted
	bool overflow_message=true;
	bool continue_on_bad_packet = false;


/* create metadata */
	uhd::rx_metadata_t md;

    boost::system_time start = boost::get_system_time();
    unsigned long long ticks_requested = (long)(time_requested * (double)boost::posix_time::time_duration::ticks_per_second());
    boost::posix_time::time_duration ticks_diff;
    boost::system_time last_update = start;
    unsigned long long last_update_samps = 0;

	boost::this_thread::sleep(boost::posix_time::seconds(1)); //allow for some setup time

	while(!stop_signal_called)
	{
		boost::system_time now = boost::get_system_time();
			//num_rx_samps = rx_stream->recv((void*)buffs, samples_per_recv, md, 0.1, uhd::device::RECV_MODE_ONE_PACKET);
			//num_rx_samps = rx_stream->recv((void*)buffs, samples_per_recv, md, uhd::io_type_t::COMPLEX_INT16,uhd::device::RECV_MODE_FULL_BUFF, 1.0);
			//num_rx_samps = rx_stream->recv(&bu.front(), bu.size(),md, 0.1, true);
			//num_rx_samps = usrp->get_device()->recv((void*)buffs, samples_per_recv, md ,*_type,uhd::device::RECV_MODE_ONE_PACKET, 1.0);
			num_rx_samps = rx_stream->recv(buffs, samples_per_recv, md, 0.1, true);


//METADATA ERRORS
	if (md.error_code == uhd::rx_metadata_t::ERROR_CODE_TIMEOUT) {
            std::cout << boost::format("Timeout while streaming") << std::endl;
			break;
        }
	if (md.error_code == uhd::rx_metadata_t::ERROR_CODE_OVERFLOW){
            if (overflow_message) {
                overflow_message = false;
                std::cout << boost::format(
                    "Got an overflow indication. Please consider the following:\n"
                    "  Your write medium must sustain a rate of %f MB/s.\n"
                    "  Dropped samples will not be written to the file.\n"
                    "  This message will not appear again.\n") % (usrp->get_rx_rate()*2*sizeof(float)/1e6);
            }
            continue;
        }
	 if (md.error_code != uhd::rx_metadata_t::ERROR_CODE_NONE){
            std::string error = str(boost::format("Receiver error: %s") % md.strerror());
            if (continue_on_bad_packet){
                std::cout << error << std::endl;
                continue;
            }
            else
                throw std::runtime_error(error);
        }


num_acc_samps += num_rx_samps;
//std::copy(buffs, samples_per_recv, std::back_inserter(bu));
        ticks_diff = now - start;
        if (ticks_requested > 0){
            if ((unsigned long long)ticks_diff.ticks() > ticks_requested)
                break;
		}
return num_rx_samps;
}
}
float usrp_src::stream_sh(unsigned int samples_per_recv, std::complex<short>* buffs)
{
	size_t num_rx_samps=0;
	size_t num_acc_samps=0;
	unsigned long long num_total_samps = 0;
	double time_requested = 3.0; // total time to receive, can also be inputted
	bool overflow_message=true;
	bool continue_on_bad_packet = false;


/* create metadata */
	uhd::rx_metadata_t md;

    boost::system_time start = boost::get_system_time();
    unsigned long long ticks_requested = (long)(time_requested * (double)boost::posix_time::time_duration::ticks_per_second());
    boost::posix_time::time_duration ticks_diff;
    boost::system_time last_update = start;
    unsigned long long last_update_samps = 0;

	boost::this_thread::sleep(boost::posix_time::seconds(1)); //allow for some setup time

	while(!stop_signal_called)
	{
		boost::system_time now = boost::get_system_time();
			//num_rx_samps = rx_stream->recv((void*)buffs, samples_per_recv, md, 0.1, uhd::device::RECV_MODE_ONE_PACKET);
			//num_rx_samps = rx_stream->recv((void*)buffs, samples_per_recv, md, uhd::io_type_t::COMPLEX_INT16,uhd::device::RECV_MODE_FULL_BUFF, 1.0);
			//num_rx_samps = rx_stream->recv(&bu.front(), bu.size(),md, 0.1, true);
			//num_rx_samps = usrp->get_device()->recv((void*)buffs, samples_per_recv, md ,*_type,uhd::device::RECV_MODE_ONE_PACKET, 1.0);
			num_rx_samps = rx_stream->recv(buffs, samples_per_recv, md, 0.1, true);


//METADATA ERRORS
	if (md.error_code == uhd::rx_metadata_t::ERROR_CODE_TIMEOUT) {
            std::cout << boost::format("Timeout while streaming") << std::endl;
			break;
        }
	if (md.error_code == uhd::rx_metadata_t::ERROR_CODE_OVERFLOW){
            if (overflow_message) {
                overflow_message = false;
                std::cout << boost::format(
                    "Got an overflow indication. Please consider the following:\n"
                    "  Your write medium must sustain a rate of %f MB/s.\n"
                    "  Dropped samples will not be written to the file.\n"
                    "  This message will not appear again.\n") % (usrp->get_rx_rate()*2*sizeof(float)/1e6);
            }
            continue;
        }
	 if (md.error_code != uhd::rx_metadata_t::ERROR_CODE_NONE){
            std::string error = str(boost::format("Receiver error: %s") % md.strerror());
            if (continue_on_bad_packet){
                std::cout << error << std::endl;
                continue;
            }
            else
                throw std::runtime_error(error);
        }


num_acc_samps += num_rx_samps;
//std::copy(buffs, samples_per_recv, std::back_inserter(bu));
        ticks_diff = now - start;
        if (ticks_requested > 0){
            if ((unsigned long long)ticks_diff.ticks() > ticks_requested)
                break;
		}
return num_rx_samps;
}
}
void usrp_src::start()
{
//	create a receive streamer
	uhd::stream_cmd_t cmd(uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS);
	cmd.num_samps=0;
	cmd.stream_now=true;
	cmd.time_spec = uhd::time_spec_t();
	rx_stream->issue_stream_cmd(cmd);
}

void usrp_src::stop()
{
	/* stop streaming */
	uhd::stream_cmd_t cmd_stop(uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS);
	rx_stream->issue_stream_cmd(cmd_stop);
	cout<<"Device Terminated";

}
