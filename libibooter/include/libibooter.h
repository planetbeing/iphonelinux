#include <usb.h>
#include <string>

namespace ibooter
{
typedef enum { 
	IB_SUCCESS = 0,
	IB_FAIL, 
	IB_DEVICE_NOT_FOUND,
	IB_FAILED_TO_OPEN,
	IB_FAILED_TO_CONFIGURE,
	IB_FAILED_TO_CLAIM,
	IB_FILE_NOT_FOUND,
	IB_CONNECTION_LOST,
	IB_COMMAND_NOT_ACK,
	IB_DUMPING_BUFFER,
} ERR_CODE;

const char *errcode_to_str(ERR_CODE code)
{
	switch (code)
	{
		case IB_SUCCESS:
			return "Success";
		case IB_FAIL:
			return "Failed";
		case IB_DEVICE_NOT_FOUND:
			return "iPhone/iTouch not found in recovery";
		case IB_FAILED_TO_OPEN:
			return "Failed to connect to iPhone/iTouch";
		case IB_FAILED_TO_CONFIGURE:
			return "You do not have adequate system privileges";
		case IB_FAILED_TO_CLAIM:
			return "Unable to claim USB device, possibly due to multiple instances running";
		case IB_FILE_NOT_FOUND:
			return "Cannot open specified file";
		case IB_CONNECTION_LOST:
			return "I/O with device failed, connection lost?";
		case IB_COMMAND_NOT_ACK:
			return "Command not acked";
		case IB_DUMPING_BUFFER:
			return "Processing reply buffer";
		default:
			return "Unknown error code";
	}
}

class CIBootConn
{
	public:

	public:
		CIBootConn(int nVendor = USB_VENDOR_ID, int nProduct = USB_PRODUCT_ID);
		~CIBootConn();

		ERR_CODE Connect();
		ERR_CODE Disconnect();
		ERR_CODE GetFile(const char *szFile, unsigned long lLoadAddr, int nLen);
		ERR_CODE SendFile(const char *szFile, unsigned long lLoadAddr);
		ERR_CODE SendCommand(const char *szCmd);
		ERR_CODE GetResponse(const char *&ppBuffer);

	private:

		typedef struct SMessage
		{
			short int cmdcode;
			short int constant;
			int 			size;
			int 			unknown;
		} SMessage;

		static const short int MSG_ECHO						= 0x801;
		static const short int MSG_DUMP_BUFFER		= 0x802;
		static const short int MSG_SEND_COMMAND		=	0x803;
		static const short int MSG_READ_FILE			= 0x804;
		static const short int MSG_SEND_FILE			=	0x805;
		static const short int MSG_CRC						=	0x807;
		static const short int MSG_ACK						=	0x808;
		static const short int MSG_REJECT					=	0x809;

	private:

		void CloseUsb();

		ERR_CODE RequestInitial(SMessage *pSend, SMessage *pRcv);
		ERR_CODE RequestSendCommand(SMessage *pSend, SMessage *pRcv, int nLen);
		ERR_CODE RequestSendFile(SMessage *pSend, SMessage *pRcv, int nLen, unsigned long lLoadAddr);
		ERR_CODE RequestReadFile(SMessage *pSend, SMessage *pRcv, int nLen, unsigned long lLoadAddr);
		ERR_CODE RequestDumpBuffer(SMessage *pSend, SMessage *pRcv);

		ERR_CODE SendControl(SMessage *pSend, SMessage *pRcv);
		ERR_CODE WriteControl(SMessage *pCtrl);
		ERR_CODE ReadControl(SMessage *pCtrl);

		ERR_CODE WriteFile(char *pBuffer, int &nLength);
		ERR_CODE ReadFile(char *pBuffer, int &nLength);

		ERR_CODE WriteSerial(char *pBuffer, int &nLength);
		ERR_CODE ReadSerial(char *pBuffer, int &nLength);

		struct usb_device *FindDevice(int nVendor, int nProduct) const;

		static int const USB_VENDOR_ID = 0x05ac;
		static int const USB_PRODUCT_ID = 0x1280;

		static int const USB_WFILE_EP = 0x05; // Write file EP
		static int const USB_RFILE_EP = 0x85; // Read file EP
		static int const USB_WCONTROL_EP = 0x04; // Write control EP
		static int const USB_RCONTROL_EP = 0x83; // Read control EP
		static int const USB_WSERIAL_EP =  0x02; // Write serial EP
		static int const USB_RSERIAL_EP = 0x81; // Read serial EP

		static int const USB_TIMEOUT = 1000;

	private:
		int 									m_nVendorId;
		int										m_nProductId;
		struct usb_dev_handle *m_pDevice;
		SMessage			 				*m_pSend, *m_pRecv;	
		std::string		 				m_sResponse;

}; // end class CIBootConn

}; // end namespace

