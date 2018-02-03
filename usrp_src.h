#ifndef USRP_SRC_H_
#define USRP_SRC_H_

#define __float128 long double
#define NDEBUG

//UHD HEADER
#include <uhd/types/tune_request.hpp>
#include <uhd/utils/thread_priority.hpp>
#include <uhd/utils/safe_main.hpp>
#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/stream.hpp>
#include <uhd/exception.hpp>
#include <uhd/config.hpp>

//BOOST HEADER
#include <boost/program_options.hpp>
#include <boost/format.hpp>
#include <boost/thread.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/ratio.hpp>
#include <boost/chrono/duration.hpp>
#include <boost/chrono/chrono.hpp>
#include <boost/chrono/time_point.hpp>
#include <boost/chrono/system_clocks.hpp>
#include <boost/chrono/typeof/boost/chrono/chrono.hpp>
#include <boost/make_shared.hpp>

#include <iostream>
#include <fstream>
#include <csignal>
#include <complex>

typedef boost::function<uhd::sensor_value_t (const std::string&)> get_sensor_fn_t;

class usrp_src{
public:
void stop();
void start();
unsigned int bufferlen();
unsigned int init(double rate, double freq, double gain, char* hostfmt, char* wirefmt);
float stream_fl(unsigned int samples_per_recv, std::complex<float>* buffs);
float stream_sh(unsigned int samples_per_recv, std::complex<short>* buffs);


private:
bool stop_signal;
bool stream_signal;

boost::shared_ptr<uhd::io_type_t> _type;

/* USRP hardware driver (UHD) */
uhd::usrp::multi_usrp::sptr usrp;
uhd::rx_streamer::sptr rx_stream;
};

#endif
