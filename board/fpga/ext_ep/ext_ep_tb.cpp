/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

#include <iostream>
#include "ext_ep.h"

#define SUBMODULE_TEST	0

using namespace std;
using namespace hls;

int print_result(int real, int expect)
{
	cout << dec << " RET: " << setw(2) << real
		    << " EXPT: " << setw(2) << expect;
	if (real == expect) {
		cout << "  SUCCESS!!" << endl;
		return 0;
	}
	else {
		cout << "  FAILED!!" << endl;
		return 1;
	}
}

int test_macro()
{
	int ret = 0;
#if SUBMODULE_TEST
	int result, tmp_lowbound, tmp_upbound;
	struct dummy2 {
		char data3;
		char data4;
	} ;
	struct dummy {
		int data1;
		struct dummy2 data2;
		char data5[2];
	};

	cout << "\nTesting mismatch macro size" << endl;
	ret |= print_result(MISMATCHSIZE + MISMATCHSIZECMPL, DATASIZE);

	cout << "\nTesting core_mem packet data size should be"
	     << " larger than deref pointer packet data size" << endl;
	result = (COREMEMDATASIZE > DEREFDATASIZE);
	ret |= print_result(result, true);

	cout << "\nTesting helper structure size, should be all"
	     << " the same as DATASIZE" << endl;
	ret |= print_result(sizeof(struct deref_req_1st), DATASIZE);
	ret |= print_result(sizeof(struct deref_req_rest), DATASIZE);
	ret |= print_result(sizeof(struct coremem_req_1st), DATASIZE);
	ret |= print_result(sizeof(struct coremem_req_rest), DATASIZE);
	ret |= print_result(sizeof(struct coremem_ret), DATASIZE);

	cout << "\nTesting lowbound and upbound macro" << endl;
	tmp_lowbound = lowbound(dummy, data2);
	tmp_upbound = upbound(tmp_lowbound, dummy, data2);
	cout << "data2 lower bound: ";
	ret |= print_result(tmp_lowbound, 32);
	cout << "data2 upper bound: ";
	ret |= print_result(tmp_upbound, 47);

	tmp_lowbound = lowbound2(dummy, data2, dummy2, data4);
	tmp_upbound = upbound(tmp_lowbound, dummy2, data4);
	cout << "data4 lower bound: ";
	ret |= print_result(tmp_lowbound, 40);
	cout << "data4 upper bound: ";
	ret |= print_result(tmp_upbound, 47);

	tmp_lowbound = lowbound(dummy, data5);
	tmp_upbound = upbound(tmp_lowbound, dummy, data5);
	cout << "data5 lower bound: ";
	ret |= print_result(tmp_lowbound, 48);
	cout << "data5 upper bound: ";
	ret |= print_result(tmp_upbound, 63);

#endif /* SUBMODULE_TEST */
	return ret;
}

int test_waitqueue()
{
	int ret = 0;
#if SUBMODULE_TEST
	WaitQueue DUT;
	uint8_t entry = 0, pop_entry = 0;

	// prepare
	for (int i = 0; i < 4; i++) {
		entry = i;
		DUT.push(entry);
		DUT.pop();
	}

	cout << "\nTesting WaitQueue push()" << endl;
	for (int i = 0; i < WAITQUEUESIZE; i++) {
		entry = i;
		DUT.push(entry);
	}
	ret |= print_result(ret, 0);

	cout << "\nTesting WaitQueue front()" << endl;
	ret |= print_result(DUT.front(), 0);

	cout << "\nTesting WaitQueue pop()" << endl;
	for (int i = 0; i < WAITQUEUESIZE; i++) {
		pop_entry = DUT.pop();
		if (i % 10 == 0)
			print_result(pop_entry, i);
	}

	cout << "\nTesting WaitQueue empty()" << endl;
	ret |= print_result(DUT.empty(), true);

#endif /* #if SUBMODULE_TEST */
	return ret;
}

void test_ext_ep()
{
	const int run_cycles = 100;
	static int delay = 5;
	stream<ap_uint<DATAWIDTH> > ext_in;
	stream<ap_uint<DATAWIDTH> > ext_out;
	ap_uint<DATAWIDTH> in = 0, out = 0;

	cout << "\nTesting whole extapi endpoint" << endl;

	// indirect read request
	in.range(hdr_req_type_up, hdr_req_type_lo) = LEGOMEM_REQ_IREAD;
	in.range(hdr_seqId_up, hdr_seqId_lo) = 0x0A;
	in.range(hdr_size_up, hdr_size_lo) = sizeof(struct legomem_deref_req);
	in.range(hdr_cont_up, hdr_cont_lo) = LEGOMEM_CONT_EXTAPI;
	in.range(deref_addr_up, deref_addr_lo) = 0x55AA55AA;
	in.range(deref_off1_up, deref_off1_lo) = 4;
	in.range(deref_off2_up, deref_off2_lo) = 8;
	in.range(deref_size_up, deref_size_lo) = 8;
	ext_in.write(in);

	for (int cycle = 0; cycle < run_cycles; cycle++) {
		ext_ep(ext_in, ext_out);

		if (ext_out.empty())
			continue;

		if (delay > 0) {
			delay--;
			continue;
		}

		out = ext_out.read();
		cout << hex << "cycle (also used as return data): 0x" << cycle << " data: " << out << endl;
		if (out.range(hdr_req_type_up, hdr_req_type_lo) == LEGOMEM_REQ_READ ||
		    out.range(hdr_req_type_up, hdr_req_type_lo) == LEGOMEM_REQ_WRITE) {
			in = 0;
			in.range(hdr_req_type_up, hdr_req_type_lo) = LEGOMEM_REQ_READ_RESP;
			in[hdr_access_cnt_bit] = out[hdr_access_cnt_bit];
			in.range(hdr_seqId_up, hdr_seqId_lo) = 0x0A;
			in.range(hdr_size_up, hdr_size_lo) = sizeof(struct lego_mem_header) + out.range(mem_size_up, mem_size_lo);
			in.range(hdr_cont_up, hdr_cont_lo) = LEGOMEM_CONT_EXTAPI;
			// return something dummy, like cycle count
			// vivado editor will produce grammar error, ignore it
			in.range((((sizeof(struct lego_mem_header) + out.range(mem_size_up, mem_size_lo)) << 3) - 1),
				 sizeof(struct lego_mem_header) << 3) = cycle;

			ext_in.write(in);
		}
		delay = 5;
	}
}

int main() {
	int ret = 0;
	ret |= test_macro();
	ret |= test_waitqueue();
	test_ext_ep();

	return ret;
}
