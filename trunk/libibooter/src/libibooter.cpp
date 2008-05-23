/*

	<insert message here/>

*/
#include "libibooter.h"
#include <cstring>
#include <cstdio>

namespace ibooter
{

CIBootConn::CIBootConn(int nVendor, int nProduct)
: m_nVendorId(nVendor), m_nProductId(nProduct), m_pDevice(NULL)
{
	m_pSend = new SMessage;
	m_pRecv = new SMessage;

	usb_init();
	usb_find_busses();
	usb_find_devices();
}

CIBootConn::~CIBootConn()
{
	Disconnect();

	delete m_pSend;
	delete m_pRecv;
}

struct usb_device *CIBootConn::FindDevice(int nVendor, int nProduct) const
{
	for(struct usb_bus *bus = usb_get_busses(); bus; bus = bus->next)
	{
		for(struct usb_device *dev = bus->devices; dev; dev = dev->next)
		{
			if(dev->descriptor.idVendor == nVendor && dev->descriptor.idProduct == nProduct)
			{
				return dev;
			}
		}
	}

	return NULL;
}

ERR_CODE CIBootConn::Connect()
{
	struct usb_device *pDevice = FindDevice(m_nVendorId, m_nProductId);
	if (!pDevice)
		return IB_DEVICE_NOT_FOUND;

	usb_dev_handle *pHandle = usb_open(pDevice);
	if (!pHandle)
		return IB_FAILED_TO_OPEN;

	m_pDevice = pHandle;
	if(usb_set_configuration(m_pDevice, 1) < 0)
	{
		CloseUsb();
		return IB_FAILED_TO_CONFIGURE;
	}

	if(usb_claim_interface(m_pDevice, 0) < 0)
	{
		CloseUsb();
		return IB_FAILED_TO_CLAIM;
	}

	ERR_CODE code;
	if ((code = RequestInitial(m_pSend, m_pRecv)) != IB_SUCCESS)
		return code;

	const char *ptr;
	GetResponse(ptr);

	return IB_SUCCESS;
}

ERR_CODE CIBootConn::Disconnect()
{
	if (m_pDevice)
	{
		usb_release_interface(m_pDevice, 0);
		CloseUsb();
	}

	return IB_SUCCESS;
}

ERR_CODE CIBootConn::GetResponse(const char *&ppBuffer)
{
	ERR_CODE code = IB_FAIL;
	m_sResponse.clear();
	ppBuffer = NULL;

	do
	{
		sleep(1);

		RequestDumpBuffer(m_pSend, m_pRecv);
		if(m_pRecv->cmdcode != MSG_ACK)
			return IB_COMMAND_NOT_ACK;

		int rcvd = m_pRecv->size;
		char *buf = new char[rcvd + 1];
		memset(buf, 0, rcvd + 1);

		if ((code = ReadSerial(buf, rcvd)) != IB_SUCCESS)
		{
			delete [] buf;
			return code;
		}

		m_sResponse += buf;

		code = IB_FAIL; 
		if (rcvd >= 4)
		{
			if ((buf[rcvd-2]==']'&&buf[rcvd-1]==' ')|| (buf[rcvd-3]==']'&&buf[rcvd-2]==' ')|| (buf[rcvd-4]==']'&&buf[rcvd-3]==' ')|| (buf[rcvd-5]==']'&&buf[rcvd-4]==' ')) 
				code = IB_SUCCESS;
			else
			{
				code = IB_DUMPING_BUFFER;
			}
		}

		delete [] buf;

	} while (code == IB_DUMPING_BUFFER);

	if (code == IB_SUCCESS)
		ppBuffer = m_sResponse.c_str();

	return code;
}

ERR_CODE CIBootConn::SendFile(const char *szFile, unsigned long lLoadAddr)
{
	FILE *sf = fopen(szFile, "r");
	if(sf == NULL)
		return IB_FILE_NOT_FOUND;

	fseek(sf, 0, SEEK_END);
	int filelen = filelen = ftell(sf);
	fseek(sf, 0, SEEK_SET);

	RequestSendFile(m_pSend, m_pRecv, filelen, lLoadAddr);
	if(m_pRecv->cmdcode != MSG_ACK)
	{
		fclose(sf);
		return IB_COMMAND_NOT_ACK;
	}

	int sent;
	char buffer[0x4000];
	do 
	{
		sent = fread(buffer, 1, sizeof(buffer), sf);
		WriteFile(buffer, sent);
	} while(sent == sizeof(buffer));
	fclose(sf);

	sprintf(buffer, "setenv filesize 0x%x\n", filelen);
	return SendCommand(buffer);
}

ERR_CODE CIBootConn::GetFile(const char *szFile, unsigned long lLoadAddr, int nLen)
{
	FILE *sf = fopen(szFile, "w");
	if (sf == NULL)
		return IB_FILE_NOT_FOUND;

	RequestSendFile(m_pSend, m_pRecv, nLen, lLoadAddr);
	if (m_pRecv->cmdcode != MSG_ACK)
	{
		fclose(sf);
		return IB_COMMAND_NOT_ACK;
	}

	int read = nLen;
	char *buf = new char[read];

	ERR_CODE code = IB_SUCCESS;
	if ((code = ReadFile(buf, read)) != IB_SUCCESS)
	{
		delete [] buf;
		fclose(sf);
		return code;
	}

	fwrite(buf, 1, read, sf);
	delete [] buf;
	fclose(sf);

	char buffer[64];
	sprintf(buffer, "setenv filesize 0x%x\n", nLen);
	return SendCommand(buffer);
}

ERR_CODE CIBootConn::WriteFile(char *pBuffer, int &nLength)
{
	int nWritten = 0;
	if ((nWritten = usb_bulk_write(m_pDevice, USB_WFILE_EP, pBuffer, nLength, USB_TIMEOUT)) < 0)
		return IB_CONNECTION_LOST;

	nLength = nWritten;
	return IB_SUCCESS;
}

ERR_CODE CIBootConn::ReadFile(char *pBuffer, int &nLength)
{
	int nRead = 0;
	if ((nRead = usb_bulk_read(m_pDevice, USB_RFILE_EP, pBuffer, nLength, USB_TIMEOUT)) < 0)
		return IB_CONNECTION_LOST;

	nLength = nRead;
	return IB_SUCCESS;
}

ERR_CODE CIBootConn::ReadControl(SMessage *pCtrl)
{
	if (usb_interrupt_read(m_pDevice, USB_RCONTROL_EP, (char *)pCtrl, sizeof(*pCtrl), USB_TIMEOUT) < 0) 
		return IB_CONNECTION_LOST;

	return IB_SUCCESS;
}

ERR_CODE CIBootConn::WriteControl(SMessage *pCtrl)
{
	if (usb_interrupt_write(m_pDevice, USB_WCONTROL_EP, (char *)pCtrl, sizeof(*pCtrl), USB_TIMEOUT) < 0)
		return IB_CONNECTION_LOST;

	return IB_SUCCESS;
}

ERR_CODE CIBootConn::SendControl(SMessage *pSend, SMessage *pRcv)
{
	ERR_CODE ret = IB_SUCCESS;

	if ((ret = WriteControl(pSend)) != IB_SUCCESS)
		return ret;

	memset(pRcv, 0, sizeof(SMessage));
	if ((ret = ReadControl(pRcv)) != IB_SUCCESS)
		return ret;

	return ret;
}

ERR_CODE CIBootConn::RequestInitial(SMessage *pSend, SMessage *pRcv)
{
	pSend->cmdcode = 0x0;
	pSend->constant = 0x1234;
	pSend->size = 0;
	pSend->unknown = 0;

	return SendControl(pSend, pRcv);
}

ERR_CODE CIBootConn::RequestSendCommand(SMessage *pSend, SMessage *pRcv, int nLen)
{
	pSend->cmdcode = MSG_SEND_COMMAND;
	pSend->constant = 0x1234;
	pSend->size = nLen;
	pSend->unknown = 0;

	return SendControl(pSend, pRcv);
}

ERR_CODE CIBootConn::RequestSendFile(SMessage *pSend, SMessage *pRcv, int nLen, unsigned long lLoadAddr)
{
	pSend->cmdcode = MSG_SEND_FILE;
	pSend->constant = 0x1234;
	pSend->size = nLen;
	pSend->unknown = lLoadAddr;

	return SendControl(pSend, pRcv);
}

ERR_CODE CIBootConn::RequestReadFile(SMessage *pSend, SMessage *pRcv, int nLen, unsigned long lLoadAddr)
{
	pSend->cmdcode = MSG_READ_FILE;
	pSend->constant = 0x1234;
	pSend->size = nLen;
	pSend->unknown = lLoadAddr;

	return SendControl(pSend, pRcv);
}

ERR_CODE CIBootConn::RequestDumpBuffer(SMessage *pSend, SMessage *pRcv)
{
	pSend->cmdcode = MSG_DUMP_BUFFER;
	pSend->constant = 0x1234;
	pSend->size = 0;
	pSend->unknown = 0;

	return SendControl(pSend, pRcv);
}

ERR_CODE CIBootConn::SendCommand(const char *pCmd)
{
	int sendlen = (int)(((strlen(pCmd)-1)/0x10)+1)*0x10;

	RequestSendCommand(m_pSend, m_pRecv, sendlen);
	if(m_pRecv->cmdcode != MSG_ACK)
		return IB_COMMAND_NOT_ACK;

	char *sendbuf = new char[sendlen];
	memset(sendbuf, 0, sendlen);
	memcpy(sendbuf, pCmd, strlen(pCmd));

	ERR_CODE code = WriteSerial(sendbuf, sendlen);
	delete [] sendbuf;

	return code;
}

ERR_CODE CIBootConn::WriteSerial(char *pBuffer, int &nLength)
{
	int nWritten = 0;
	if ((nWritten = usb_bulk_write(m_pDevice, USB_WSERIAL_EP, pBuffer, nLength, USB_TIMEOUT)) < 0)
		return IB_CONNECTION_LOST;

	nLength = nWritten;
	return IB_SUCCESS;
}

ERR_CODE CIBootConn::ReadSerial(char *pBuffer, int &nLength)
{
	int nRead = 0;
	if ((nRead = usb_bulk_read(m_pDevice, USB_RSERIAL_EP, pBuffer, nLength, USB_TIMEOUT)) < 0)
		return IB_CONNECTION_LOST;

	nLength = nRead;
	return IB_SUCCESS;
}

void CIBootConn::CloseUsb()
{
	usb_close(m_pDevice);
	m_pDevice = NULL;
}

};
