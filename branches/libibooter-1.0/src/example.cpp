#include "libibooter.h"
#include <iostream>

using namespace ibooter;
using namespace std;

int main(int argv, char **argc) 
{
	CIBootConn conn;
	ERR_CODE code;
	cout << "Connecting..." << endl;
	if ((code = conn.Connect()) != IB_SUCCESS)
	{
		cout << errcode_to_str(code) << endl;
		return 1;
	}

	cout << "Sending command..." << endl;
	if ((code = conn.SendCommand("printenv\n")) != IB_SUCCESS)
	{
		cout << errcode_to_str(code) << endl;
		conn.Disconnect();
		return 1;
	}

	const char *pResponse = NULL;
	if ((code = conn.GetResponse(pResponse)) != IB_SUCCESS)
	{
		cout << errcode_to_str(code) << endl;
		conn.Disconnect();
		return 1;
	}

	cout << "Response: " << pResponse << endl;
	conn.Disconnect();

	return 0;
}
