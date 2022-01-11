/*
 * at_parser.h
 *
 *  Created on: Mar 14, 2020
 *      Author: J.cliff
 */

#ifndef ATC_H_
#define ATC_H_

#define SEND_GET_UART_MSG_MAX_DELAY 100


//TODO: increase clients number to 16 for both wifi and bluetooth
//#define MAX_NO_OF_BLE_CLIENTS 8
//#define MAX_NO_OF_WIFI_CLIENTS 8
#define MAX_NO_OF_AT_CLIENTS 8 //MAX_NO_OF_WIFI_CLIENTS + MAX_NO_OF_BLE_CLIENTS


/* brought this here to solve a circular reference issue*/
class Clients{
public:
	typedef enum {
		WIFI,
		BLE
	}Client_type;

	bool conn;

	int session_id;		//id received from wifi app
	int conn_id;		//id of the client connected to the wifi
	char uuid[30]; 		//Unique id from the esp32 module

	Client_type type;

	void setSessionId(int sessId){
		session_id = sessId;
	}
	void setConnId(int conId){
		conn_id = conId;
	}

	Clients();
	void close();
	void open(int con_id);


	/** FOr ble */
	int servIndex;
	int writeXticsIndex;
	int readXticsIndex;


};

class Clients_ble: public Clients{
public:
	Clients_ble();
	~Clients_ble();
};


#include <string>
#include <cstdlib>
#include <main.h>




class Uart;


//#define MAX_NO_OF_AT_CLIENTS	4

using namespace std;

typedef u16 (*uart_write_fn_cb)(u8*,u32);

typedef enum{
	AT_UNKNOWN = -1,
	AT_CMD = 0,
	AT_DATA,
}ATState;

typedef enum{
	C_MODE_UNKNOWN = -1,
	C_MODE_SINGLE_CLIENT = 0,
	C_MODE_MULTI_CLIENT
}multi_conn_mode;

typedef enum{
	OP_MODE_UNKNOWN = -1,
	OP_MODE_STATION = 1,
	OP_MODE_AP,
	OP_MODE_AP_STATION
}Operation_mode_enum;		//main operation mode

typedef enum{
	TRANS_MODE_UNKOWN = 0,
	TRANS_MODE_NORMAL,
	TRANS_MODE_TRANSPARENT
}trans_mode_enum;		//transmission mode

typedef enum{
	C_UNKNOWN,
	C_OPEN = 0,
	C_WPA_PSK = 2,
	C_WPA2_PSK,
	WPA_WPA2_PSK
}ecn_enum; //security type

typedef enum{
	AT_CMD_NONE = -1,

	AT_CMD_RESET = 0,
	AT_CMD_KEEP_ALIVE,
	AT_CMD_ECHO_OFF,
	AT_CMD_ECHO_ON,

	AT_CMD_CHECK_OP_MODE,
	AT_CMD_SET_OP_MODE,

	AT_CMD_CHECK_CONN_MODE,
	AT_CMD_SET_CONN_MODE,

	AT_CMD_CHECK_AP_INFO,
	AT_CMD_SET_AP_INFO,

	AT_CMD_CHECK_AP_IP,
	AT_CMD_SET_AP_IP,

	AT_CMD_CHECK_TRANS_MODE,
	AT_CMD_SET_TRANS_MODE,

	AT_CMD_SET_CONN_TIMEOUT,

	AT_CMD_START_SERVER,

	AT_CMD_DATA,

	/** at BLE commands*/
	AT_CMD_BLE_INIT,
	AT_CMD_BLE_GATT_SERVICES_CREATE,
	AT_CMD_BLE_GATT_SERVICES_START,
	AT_CMD_BLE_SET_NAME,
	AT_CMD_BLE_ADVERTIZING_START,
}AT_cmd_enum;

typedef enum{
	AT_R_NO_DATA = -1,		// this is actually reserved for the wait_for_response function which returns -1 as need be
	AT_R_UNDEFINED = 0,
	AT_R_OK,
	AT_R_BUSY,
	AT_R_BUSY_SENDING,
	AT_R_SEND_OK,
	AT_R_SEND_FAIL,
	AT_R_PROMPT,
	AT_R_IPD,
	AT_R_CONNECT,
	AT_R_CLOSED,
	AT_R_ERROR_LINK_NOT_VALID,
	AT_R_ERROR,
	AT_R_AT_ECHO,
	AT_R_CONNECT_FAIL,

	AT_R_IPD_INCOMP,

	/** BLE resps */
	AT_R_WRITE,
	AT_R_BLE_CONN,
	AT_R_BLE_ADDR

}AT_cmd_resp_enum;

typedef enum{
	AT_C_CH_CLOSED,
	AT_C_CH_CONNECTED,
}AT_client_ch_enum;		//client channel enum




/** @defgroup ATprotocol Class.
  * @{ */

/**
 * AT protocol can be used with several uart ports, thats the main reason for making it a class
 * and making the formal static variables and function members regular members
 * Only one AT parser object should be instantiated for a uart port
 *
 *  The wifi variable would only make use of an instantiated AT object (it won't be instantiated within it)
 */

class ATprotocol{
public:
	ATState state;
	char name[20];
	ATprotocol();

	char addressBuffer[50];
	int addressBufferLen;

/*	Interface functions  */
	/* these exchange data with the device uart functions */
	uart_write_fn_cb uart_write_fn_ptr;		//function variable to send data to uart
	u16 uart_write_fn(u8* data, u32 data_len);
	Uart *wifi_uart_obj;
	void sysSetup(uart_write_fn_cb uart_write_fn, Uart *wifi_uart_o = 0);


	/* these exchange data with the user application (wifi thread) */
	stringstream uart_input_stream;
	char uart_c_input_stream[2046];
	char c_temp[2046];
	//client no.

	void read_into_uart_stream(char* data, int data_len);
	string read_from_uart_stream();
	void flush_input_stream();
	bool data_available;

	stringstream cmd_stream;
	void read_into_cmd_buff(int cmd);
	int read_from_cmd_buff();
	bool cmd_available;
	void flush_cmd_input_buff();

	stringstream client_data_stream;
//	char client_data_stream[5024];
	void read_into_client_data_buff(char* raw_data);
	string read_from_client_buff(Clients** client);		//take the pointer itself as a variable to update
	bool client_data_available;
	void flush_client_input_buff();

/* Commands and messages functions */
	//channel state, 4 is max no. of possible channels however i used 8 for scalibilty purpose

	Clients client[MAX_NO_OF_AT_CLIENTS+1];	//the last is the null client
	Clients_ble client_ble[MAX_NO_OF_AT_CLIENTS];	//the last is the null client

	Clients *new_client;						//new_client stores the pointer to the most recent client object
	Clients null_client;			//an active client in the wifi object is pointed to this null_client if it is not active

	void closeClientsExcept(int con_id);
	/* had to include this back as opposed to opening a client with only its open() method,
	 * this ATprotocol parent method makes it possible to assign values to the newclient variable ptr*/
	bool openClient(int con_id, char* uuid = NULL, Clients::Client_type client = Clients::WIFI);

	//configuration variables
//	Operation_mode_enum op_mode;
//	multi_conn_mode conn_mode;
//	trans_mode_enum transmission_mode;

	//Soft access point info
	string ssid;
	string pswd;
	u8 channel;
	ecn_enum security_type;

	//server info
	string ip;
	string gwip;
	u16 port;

	//communication info
	AT_cmd_resp_enum AT_resp_type;
	u32 con_conn_id;	//connection id from "connect" msg, used to keep track of the client object

	/**
	 * Function group: 	sends preconfigured commands to the module
	 * 					their return value tells if the command execution is successful or not
	 *
	 * @param cw 		operation mode value, 1: Station, 2: AP, 3; AP and Station
	 * @param cm		multiconnection value, 0: single client, 1: multi client
	 * @param ssid		SSID
	 * @param psw		password
	 * @param ch		channel (use 1)
	 * @param st		security type (see enum for values)
	 * @param ip		ip
	 * @param tm		switch between normal mode and transparent mode (see enum for values)
	 * @param conn_no	connection no, 0: disconnect, 1: create server with the "port" (no enum for this)
	 * @param port		port to connect to
	 * @param sec 		timeout is seconds
	 * @param msg		msg to send out (c++ string)
	 * @param cmd 		timeout is seconds
	 * @param cmd 		timeout is seconds
	 */

	int keepModuleAlive();

	int setEchoOff();
	int setEchoOn();

	int checKCWmode(); //returns Operation_mode_enum object
	int setCWmode(Operation_mode_enum cw, int def = 1);//def = 1, either include default or not

	int checkMultiConnMode(); //returns multi_conn_mode object
	int setCIPmode(multi_conn_mode cm);

	int setCIPclose(int id);

	int checkAPinfo();
	int setAPinfo(string ssid, string psw, u32 ch, ecn_enum st, int def=1);

	int checkAPIP();
	int setAPIP(string ip, int def = 1);

	int checkTransMode();
	int setTransMode(trans_mode_enum tm);

	int reset();
	int startServer(u32 conn_no, u16 port);
//	int checkServer();

	int setConTimeout(int sec);

	/**
	 * APIs for BLE
	 */
//	int bleSendCmd(AT_cmd_enum c);
	typedef enum{
		INDICATION,
		NOTIFY,
	}ble_write_type;


	int bleDeInit();
	int bleInitServer();
	int bleCreateGATTServices();
	int bleStartGATTServices();
	int bleSetAdvertData(std::string name); //only accept name only for now
	int bleSetName(std::string name);
	int bleStartAdvertizing();
	int bleGetAddress();

	int bleWriteReq(int client_id, ble_write_type t, int servIndex, int writeXticsIndex, int len);
	int bleWriteData(uint8_t* d, int len);



	/** New scanning APIs*/
	bool isMsgBleConn(const char* msg);
	bool isMsgBleWrite(std::string msg, std::string& msg_out);

	/**
	 * This function checks the new data flag and group incoming messages as either a data from client
	 * or a command response
	 */
	void thread_function();

	/**
	 * send data to a particular client
	 * The functions above are used to send cmd to the wifi module
	 *
	 * */
	int send_data_to_client(string msg_str, Clients* client = NULL);		//sending to a specific conn_id

	/**
	 * Send data and wait for a response from the wifi module
	 *
	 * @param	write_buf	data to send to wifi module
	 * @param	size		size of data
	 * @param	duration	how long to wait for a response from module after sending the data
	 * 						default value is SEND_GET_UART_MSG_MAX_DELAY 1000
	 *
	 * @return	AT_cmd_resp_enum	message from the processed msg (fn is below)
	 * 								returns -1 if no messae came in
	 */
	int send_cmd_and_get_resp(unsigned char* write_buf, unsigned int size, int duration_ms = SEND_GET_UART_MSG_MAX_DELAY);

	/**
	 *
	 * wait reply to command from the wifi module
	 *
	 * @param	duration_ms	how 	long to wait
	 * @param	bypass_send_..		a redundant flag, check it properly and remove
	 * 								It is suppose to keep message clash between threads
	 *
	 * @return	AT_cmd_resp_enum	message from the processed msg (fn is below)
	 * 				0 				if a message came in before wait time expires
	 * 				-1 				if no message came in
	 */

	int wait_for_cmd_resp(int duration_ms = SEND_GET_UART_MSG_MAX_DELAY);

		/**
		 * Processes the received data and enumerate it if its a command reply or data
		 *
		 * @return	AT_cmd_resp_enum 		The type of msg that came in
		 * 				AT_R_NO_DATA	-1 	No data (tied to the -1 other functions)
		 * 				AT_R_UNDEFINED	0	undefined message
		 * 				AT_R_IPD		7	data from a client
		 * 				etc.
		 */
		int process_received_msg(string str, string &out_str);

			/**
			 * Parses the client data if AT_R_IPD msg is received
			 *
			 * @param[in]	msg		raw unparsed message from wifi module
			 * @param[out]	msgLen	get the len of messgae received
			 * @param[out]	conn_id	get the connection of received data
			 * @return 				client string processed
			 */

			/**
			 * if IPD msg is received tries to get the length of the msg sent from esp
			 *
			 * @param[in]	msg		raw unparsed message from wifi module
			 * @return 				length of msg plus length of the head "+IPD,0,10:"
			 * @return  	-1 + length 	if can't find last index
			 */
			string getIncomingData(string msg,  u32* msgLen, u32* conn_id);

			int32_t getMsgLen(string r_msg);


			bool parseWifiAtMsg(std::string r_msg, std::string& msg_out, u32* msgLen, u32* conn_id);

			bool parseBleAtMsg(std::string r_msg, std::string& msg_out, u32* msgLen, u32* conn_id);

			int parseAddressData(std::string r_msg, char* msg_out, int msg_size);

////			int32_t getBleMsgLen(string r_msg);
//			int32_t getBleMsgLen(std::string r_msg, int32_t* total_out = NULL);
//			int32_t getBleMsgString(std::string r_msg, char* msg_out= NULL, int16_t msg_size = 0);

};


/**
 * Ble message parser
 */

class BleMsgParser
{

public:
	BleMsgParser(const char* msg_str);
	~BleMsgParser();

	const char** msgStr;
	int16_t tk[10];
	int setMsgStr(const char* msg_in);

	int tokenize();
	int readNumAtTkn(int tk_index);
	int getMsglen();
	int getConnid();
	int getServid();
	int getXticsid();
	int getIndictnId();
	std::string getMsgPayload(char* msg_out = NULL, int size = NULL);

};


#endif /* ATC_H_ */


