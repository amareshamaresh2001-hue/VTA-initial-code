#include "tb.h"
#include <fstream>

void tb::source()
{
	// Reset
	inp.write(0);
	inp_vld.write(0);
	rst.write(1);
	wait();
	rst.write(0);
	wait();

	sc_int<16> tmp;
	// Send stimulus to FIR
	for (int i = 0; i < 64; i++)
	{
		if (i > 23 && i < 29)
			tmp = 256;
		else
			tmp = 0;

		inp_vld.write(1);
		inp.write(tmp);
		do
		{
			wait();
		} while (!inp_rdy.read());
		inp_vld.write(0);
	}
}

void tb::sink()
{
	sc_int<16> indata;

	std::ofstream logging("log.txt");

	// Initialize port
	outp_rdy.write(0);

	// Read output comming from DUT
	for (int i = 0; i < 64; i++)
	{
		outp_rdy.write(1);
		do
		{
			wait();
		} while (!outp_vld.read());
		indata = outp.read();
		outp_rdy.write(0);

		cout << i << " :\t" << indata.to_int() << " \t" << sc_core::sc_time_stamp().to_double() << endl;
		logging << i << " :\t" << indata.to_int() << endl;
		wait();
	}

	logging.close();

	// End simulation
	sc_stop();
}