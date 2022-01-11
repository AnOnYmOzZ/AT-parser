/*
 * ATC.cpp
 *
 *  Created on: Mar 14, 2020
 *      Author: J.cliff
 */

#include <main.h>
#include "at_parser.h"
#include "uart_listener.h"
#include "ccp.h"

uint8_t AT_sem_name[] = "AT_PARSER";
EatSemId_st AT_sem = eat_create_sem(AT_sem_name, 1);
//while(!eat_sem_get(AT_sem, EAT_INFINITE_WAIT));
//eat_sem_put(AT_sem);


void ATprotocol::sysSetup(uart_write_fn_cb uart_write_fn_var
		, Uart *wifi_uart_o){

	if (wifi_uart_o == 0){
		uart_write_fn_ptr = uart_write_fn_var;
		return;
	}
	//if there is a value present for wifi_uart_o in order to set
	wifi_uart_obj = wifi_uart_o;
	wifi_uart_obj->ATuart = this;
}

u16 ATprotocol::uart_write_fn(u8* data, u32 data_len){
	if(wifi_uart_obj != (void*)0){
		return wifi_uart_obj->writed((char*)data, data_len);
	}
	if (uart_write_fn_ptr != (void*)0){
		return uart_write_fn_ptr(data, data_len);
	}

	DEBUG_ATC("[%s] VOID NO fn / UART obj to write to [%d]"
			, __FUNCTION__, data_len);
	return 0;
}

/*	Interface variables and functions: to be used by the wifi thread	*/

void ATprotocol::read_into_uart_stream(char* data, int data_len){
	//	while(!eat_sem_get(AT_sem, EAT_INFINITE_WAIT));
	strcat(uart_c_input_stream, data);
	data_available = true;
	DEBUG_ATC("<%s> len: %d", __FUNCTION__, strlen(uart_c_input_stream));
	//	eat_sem_put(AT_sem);
}

void ATprotocol::flush_input_stream(){
	memset(uart_c_input_stream, 0, sizeof(uart_c_input_stream)/sizeof(uart_c_input_stream[0]));
	DEBUG_ATC("<%s>, str: %s", __FUNCTION__, uart_c_input_stream);
	data_available=false;
}

string ATprotocol::read_from_uart_stream(){

	/**************************** using c_tokenizer *******************************/
	string str_out;
	char * tok_ptr;
	strcpy(c_temp, uart_c_input_stream);
	char * token2;
	char *token = strtok_r(c_temp, "\r\n", &tok_ptr);
	if (token != NULL){
		str_out = token;
		token2 = strtok_r(NULL, "\r\n", &tok_ptr);
		if (token2 == NULL ){
			DEBUG_ATC("end of string reached");
			data_available = false;
			memset(uart_c_input_stream, 0, sizeof(uart_c_input_stream)/sizeof(uart_c_input_stream[0]));
			//			memmove(uart_c_input_stream, uart_c_input_stream+strlen(token), strlen(uart_c_input_stream)+1);
		}
		else {
			DEBUG_ATC("still going ");
			data_available = true;
			memmove(uart_c_input_stream, token2, strlen(uart_c_input_stream)+1);
		}
	}else {
		data_available = false;
	}
	DEBUG_ATC("[%s::%s after> len: %d, str_out: %s, data_avalable: %d, next msg: %s"
			, name, __FUNCTION__, strlen(uart_c_input_stream), str_out, data_available, uart_c_input_stream);

	return str_out;
}


/* AT functions and variables */
int ATprotocol::keepModuleAlive(){
	string str = "AT\r\n";
	return send_cmd_and_get_resp((u8*)str.c_str(),str.length());
}

int ATprotocol::setEchoOff(){
	int ret = 0;
	int cnt = 3;
	string str = "ATE0\r\n";

	ret = send_cmd_and_get_resp((u8*)str.c_str(),str.length());
	/* This second layer of listening is used in order to receive the OK msg after
	 * switching off echo */
	while (ret != AT_R_OK && --cnt > 0){
		ret = wait_for_cmd_resp(100);
		DEBUG_ATC("<%s> while wait for OK, cnt: %d, ret: %d",__FUNCTION__, cnt, ret);
	}
	return ret;
}

int ATprotocol::setEchoOn(){
	string str = "ATE1\r\n";
	return send_cmd_and_get_resp((u8*)str.c_str(),str.length());
}

int ATprotocol::checKCWmode(){
	string str = "AT+CWMODE?\r\n";
	return send_cmd_and_get_resp((u8*)str.c_str(),str.length());
}

int ATprotocol::setCWmode(Operation_mode_enum cw, int def){
	stringstream ss;
	ss << "AT+CWMODE";
	if (def) ss << "_DEF";
	ss << "=" << cw << "\r\n";
	return send_cmd_and_get_resp((u8*)ss.str().c_str(),ss.str().length());
}

int ATprotocol::checkMultiConnMode(){
	string str = "AT+CIPMUX?\r\n";
	return send_cmd_and_get_resp((u8*)str.c_str(),str.length());
}

int ATprotocol::setCIPmode(multi_conn_mode cm){
	stringstream sstr;
	sstr << "AT+CIPMUX="<< cm << "\r\n";
	return send_cmd_and_get_resp((u8*)sstr.str().c_str(),sstr.str().length());
}

int ATprotocol::setCIPclose(int id){
	stringstream sstr;
	sstr << "AT+CIPCLOSE="<< id << "\r\n";
	return send_cmd_and_get_resp((u8*)sstr.str().c_str(),sstr.str().length());
}

int ATprotocol::checkAPinfo(){
	string str = "AT+CWSAP?\r\n";
	return send_cmd_and_get_resp((u8*)str.c_str(),str.length());
}

int ATprotocol::setAPinfo(string ssid, string psw, u32 ch, ecn_enum st, int def){
	//	"AT+CWSAP=\""+ssid+"\",\""+psw+"\","+ch+","+st+"\r\n";
	stringstream sstr;
	sstr << "AT+CWSAP";
	if (def) sstr << "_DEF";
	sstr << "=\"" << ssid<<"\",\""<<psw<<"\","<<ch<<","<<st<<"\r\n";
	//	sstr << "=\""<<ssid<<"\",\""<<psw<<"\","<<ch<<","<<st<<",4"<<",1"<<"\r\n";
	//This second command is used to set the ap to hidden mode
	return send_cmd_and_get_resp((u8*)sstr.str().c_str(),sstr.str().length(), 4000);
}

int ATprotocol::checkAPIP(){
	string str = "AT+CIPAP?\r\n";
	return send_cmd_and_get_resp((u8*)str.c_str(),str.length());
}

int ATprotocol::setAPIP(string ip, int def){
	stringstream sstr;
	sstr << "AT+CIPAP";
	if (def) sstr << "_DEF";
	sstr << "=\""<<ip<<"\"\r\n";
	return send_cmd_and_get_resp((u8*)sstr.str().c_str(),sstr.str().length(), 2000);
}

int ATprotocol::checkTransMode(){
	string str = "AT+CIPMODE?\r\n";
	return send_cmd_and_get_resp((u8*)str.c_str(),str.length());
}

int ATprotocol::setTransMode(trans_mode_enum tm){
	stringstream sstr;
	sstr << "AT+CIPMODE=\""<<tm<<"\"\r\n";
	return send_cmd_and_get_resp((u8*)sstr.str().c_str(),sstr.str().length());
}

int ATprotocol::startServer(u32 conn_no, u16 port){
	stringstream sstr;
	sstr <<	"AT+CIPSERVER="<<conn_no<<","<<port<<"\r\n"; //"AT+CIPMODE?\r\n";
	string str = sstr.str();
	return send_cmd_and_get_resp((u8*)str.c_str(),str.length(), 2000);
}

//int ATprotocol::checkServer(){
//	string str = "AT+CIPMODE?\r\n";
//	return send_cmd_and_get_resp((u8*)str.c_str(),str.length());
//}

int ATprotocol::setConTimeout(int sec){
	ostringstream oss;
	oss << sec;
	string str = "AT+CIPSTO="+oss.str()+"\r\n";
	return send_cmd_and_get_resp((u8*)str.c_str(),str.length());
}

/**
 * *****************************
 * APIs for BLE
 * *****************************
 */

//int bleSendCmd(AT_cmd_enum c)
//{;
//}

int ATprotocol::bleDeInit()
{
	stringstream sstr;
	sstr <<	"AT+BLEINIT=0\r\n";
	string str = sstr.str();
	return send_cmd_and_get_resp((u8*)str.c_str(),str.length(), 2000);
}

int ATprotocol::bleInitServer(){
	stringstream sstr;
	sstr <<	"AT+BLEINIT=2\r\n";
	string str = sstr.str();
	return send_cmd_and_get_resp((u8*)str.c_str(),str.length(), 2000);
}

int ATprotocol::bleCreateGATTServices(){
	stringstream sstr;
	sstr <<	"AT+BLEGATTSSRVCRE\r\n";
	string str = sstr.str();
	return send_cmd_and_get_resp((u8*)str.c_str(),str.length(), 1000);
}

int ATprotocol::bleStartGATTServices(){
	stringstream sstr;
	sstr <<	"AT+BLEGATTSSRVSTART\r\n";
	string str = sstr.str();
	return send_cmd_and_get_resp((u8*)str.c_str(),str.length(), 1000);
}

int ATprotocol::bleSetAdvertData(std::string nm)
{//only accept name only for now
	stringstream sstr;
	sstr <<	"AT+BLEADVDATAEX=\""<< nm <<"\",\"A002\",\"0011223344\",0\r\n";
	string str = sstr.str();
	return send_cmd_and_get_resp((u8*)str.c_str(),str.length(), 1000);
}

int ATprotocol::bleSetName(std::string nm){
	stringstream sstr;
	sstr <<	"AT+BLENAME=\""<< nm <<"\"\r\n";
	string str = sstr.str();
	return send_cmd_and_get_resp((u8*)str.c_str(),str.length(), 1000);
}

int ATprotocol::bleStartAdvertizing(){
	stringstream sstr;
	sstr <<	"AT+BLEADVSTART\r\n";
	string str = sstr.str();
	return send_cmd_and_get_resp((u8*)str.c_str(),str.length(), 1000);
}

int ATprotocol::bleGetAddress()
{
	stringstream sstr;
	sstr <<	"AT+BLEADDR?\r\n";
	string str = sstr.str();
	return send_cmd_and_get_resp((u8*)str.c_str(),str.length(), 1000);
}

int ATprotocol::bleWriteReq(int client_id, ble_write_type t, int servIndex, int writeXticsIndex, int len)
{
	stringstream sstr;
	sstr <<	"AT+BLEGATTS";
	switch (t)
	{
	case INDICATION:
		sstr << "IND";
		break;
	case NOTIFY:
		sstr << "NTFY";
		break;
	default:
		break;
	}
	sstr <<	"=" << client_id
			<< "," << servIndex
			<< "," << writeXticsIndex
			<< "," << len
			<<	"\r\n";

	string str = sstr.str();
	return send_cmd_and_get_resp((u8*)str.c_str(),str.length(), 1000);
}

int ATprotocol::bleWriteData(uint8_t* data, int len)
{
	return send_cmd_and_get_resp(data, len, 1000);
}


/** New scanning APIs*/
bool ATprotocol::isMsgBleConn(const char* msg)
{
	/** Total number of parameters: 2 [used in return value]
	 *  Total lenght of uuid string: 17 [used in sscanf format] */
	int con_id = 0;
	char uuid_str[50] = {0};

	int ret = sscanf(msg, "+BLECONN:%d,\"%17s\"", &con_id, uuid_str);

	if (ret != 2 ) return false;

	DEBUG_ATC ("[%s]\"+BLECONN\" found (sscanf), ret: %d, conn id: %d, uuid: %s, msg: %s\n"
			, __FUNCTION__, ret, con_id, uuid_str, msg);

	openClient( con_id, uuid_str, Clients::BLE);

	return true;

}

bool ATprotocol::isMsgBleWrite(std::string msg, std::string& msg_out)
{
	if (strstr(msg.c_str(), "+WRITE:") == NULL) return false; //+WRITE:1,1,5,,7,jgghh
	msg_out = msg;
	return true;
}

/**
 * *****************************
 * Other apis
 * *****************************
 * */

int ATprotocol::reset(){
	//	uart_write((u8*)"+++",strlen("+++"));
	//use a fn pointer in order ti pick the data output function of the application
	string str = "AT+RST\r\n";
	return send_cmd_and_get_resp((u8*)str.c_str(),str.length());
}

int ATprotocol::send_data_to_client(string msg_str, Clients* client){
	msg_str = msg_str + "\r\n";

	Clients * cli;
	int ret = 0;	//what to return depend on the success or failure of sending a particular message
	int at_resp;

	cli = (client == NULL) ? new_client : client;	//if it is the defualt value null
	if (!cli->conn) return -77;

	while(!eat_sem_get(AT_sem, EAT_INFINITE_WAIT));

	eat_sleep(10);

	DEBUG_ATC("<%s> pump wifi clients ptr: %x, used cli: %x, cli con_id: %d"
			,__FUNCTION__, client, cli, cli->conn_id);

	stringstream sstr;

	if (cli->type == Clients::WIFI)
	{
		sstr << "AT+CIPSEND="<<cli->conn_id<<","<<msg_str.length()<<"\r\n";
	}
	else
	{	sstr << "AT+BLEGATTSNTFY="
				<< cli->conn_id <<","
				<< cli->servIndex << ","
				<< cli->writeXticsIndex << ","
				<< msg_str.length()<<"\r\n";
	}

	string str = sstr.str();

	int cnt = 5;
	at_resp = send_cmd_and_get_resp((u8*)str.c_str(),str.length());

	int last_at_resp = at_resp;

	while(--cnt)
	{
		DEBUG_ATC("<%s> WHILE WAITING for REPLY cnt: %d, ret: %d",__FUNCTION__, cnt, at_resp);
		if (at_resp == AT_R_OK){
			at_resp = wait_for_cmd_resp();
			DEBUG_ATC("<%s> AT-OK cnt: %d, ret: %d",__FUNCTION__, cnt, at_resp);
		}
		else if(at_resp == AT_R_PROMPT){
			at_resp = send_cmd_and_get_resp((u8*)msg_str.c_str(),msg_str.length());
			DEBUG_ATC("<%s> AT-PROMPT cnt: %d, ret: %d",__FUNCTION__, cnt, at_resp);
			if(at_resp == AT_R_OK ){ //If you get OK after sending data, for wifi it returns SEND_OK
				DEBUG_ATC("<%s> AT-PROMPT -> at_resp returned OK, cnt: %d, ret: %d",__FUNCTION__, cnt, at_resp);
				break;
			}

		}
		else if(at_resp == AT_R_BUSY_SENDING){
			at_resp = wait_for_cmd_resp();
			DEBUG_ATC("<%s> AT-BUSY-SENDING cnt: %d, ret: %d",__FUNCTION__, cnt, at_resp);
		}
		else if(at_resp == AT_R_SEND_FAIL ){
			DEBUG_ATC("<%s> AT-SEND FAIL cnt: %d, ret: %d",__FUNCTION__, cnt, at_resp);
			break;
		}
		else if(at_resp == AT_R_SEND_OK ){
			DEBUG_ATC("<%s> AT-SEND OK cnt: %d, ret: %d",__FUNCTION__, cnt, at_resp);
			break;
		}
		else if (at_resp == AT_R_ERROR_LINK_NOT_VALID){
			DEBUG_ATC("<%s> AT-LINK NOT VALID cnt: %d, ret: %d",__FUNCTION__, cnt, at_resp);
			break;
		}
		else {
			DEBUG_ATC("<%s> AT RESP if condtn not defined: %d, ret: %d",__FUNCTION__, cnt, at_resp);
			at_resp = wait_for_cmd_resp();
		}
	}
	if (cnt == 0 && at_resp == AT_R_NO_DATA){
		uart_write_fn((u8*)"+++",strlen("+++"));	//danger management piece of code
	}

	DEBUG_ATC("[%s::%s] %s, ret: %d",
			name, __FUNCTION__, msg_str, at_resp);

	eat_sem_put(AT_sem);

	return at_resp;
}

/** *********************************
 * XXX: 1 managing broken ipd messages */
stringstream ipd_buff_temp;
uint32_t ipd_last_time = 0;
uint32_t ipd_duration = 0;
#define IPD_TIMEOUT 130 // if next ipd message isn't gotten in 500ms
/** *********************************
 * managing broken ipd messages - END variables declaration */


int ATprotocol::process_received_msg(string msg, string &out_str){
	DEBUG_ATC("[%s::%s] wifi RESPONSE: %s, length: %d",
			name, __FUNCTION__, msg, msg.length());
	char *tok_ptr1;
	char *tok_ptr2;
	out_str = "";
	AT_resp_type = AT_R_UNDEFINED;

	if (msg == "OK"){
		AT_resp_type = AT_R_OK;
		DEBUG_ATC("[%s::%s]OK found (method 1): %s",
				name, __FUNCTION__, msg);
	}

	else if (msg == "SEND OK"){
		AT_resp_type = AT_R_SEND_OK;
		DEBUG_ATC("[%s::%s]SEND OK found!! : %s",
				name, __FUNCTION__, msg);
	}
	//find +IPD
	else if (strstr(msg.c_str(), "+IPD") != NULL){ 		//length of the string == 4

		int32_t l = getMsgLen(msg); /** l here is the frame length + header length + length of ':'*/
		if ( l+1 < msg.length() || msg.length()  < l-1){//damage control for +1 or -1 error
			//<- UART2 [69]b '+IPD,0,57:{"cm":"sv","pn":"P1","au":true}{"tk":2104176683,"st":0}'
			// msg.length() = 69, l = 57, l+12 = 69
			//broken msg: msg.length = 34, l+12 = 69
			ipd_buff_temp << msg;
			ipd_last_time = eat_get_current_time();
			//			AT_resp_type = AT_R_IPD_INCOMP;s
		}else {
			ipd_buff_temp.str("");	//clear buffer
			out_str = msg;	//do not parse this message yet
			AT_resp_type = AT_R_IPD;
		}

		/**XXX: 2 managing broken ipd msgs */
		DEBUG_ATC("[%s::%s]+IPD found!! con id: %d, f: %d, full data len: %d, msg: %s ",
				name, __FUNCTION__, new_client->conn_id, l, msg.length(), msg);
	}

	/** Attending to BLE messages received */
	else if ( isMsgBleWrite( msg, out_str) )
	{
		AT_resp_type = AT_R_WRITE;
	}

	else if (strstr(msg.c_str(),"link is not valid")){
		AT_resp_type = AT_R_ERROR_LINK_NOT_VALID;
		new_client->close();
		DEBUG_ATC("[%s::%s]link is not valid!! : %s",
				name, __FUNCTION__, msg);
	}

	else if (msg == "> " || msg == ">"){
		AT_resp_type = AT_R_PROMPT;
		DEBUG_ATC("[%s::%s] > found!! : %s",
				name, __FUNCTION__, msg);
	}

	else if (strstr(msg.c_str(),"busy s...")){
		AT_resp_type = AT_R_BUSY_SENDING;
		DEBUG_ATC("[%s::%s] BUSY SENDING found!!: %s",
				name, __FUNCTION__, msg);
	}

	else if (strstr(msg.c_str(), "ERROR") != NULL){
		AT_resp_type = AT_R_ERROR;
		DEBUG_ATC("[%s::%s] ERROR found!! : %s",
				name, __FUNCTION__, msg);
	}

	//if connect message came in
	else if (strstr(msg.c_str(), "CONNECT FAIL") != NULL){ 		//length of the string == 4
		char *tk = strtok_r((char*)msg.c_str(),",", &tok_ptr2);
		if (tk != NULL){
			con_conn_id = atoi(tk);
		}
		if (con_conn_id >=0 && con_conn_id < 8) {
			client[con_conn_id].close(); //close new client
		}
		AT_resp_type = AT_R_CONNECT_FAIL;
		DEBUG_ATC("[%s::%s] CONNECT FAIL! found!!: %s, connection id: %d",
				name, __FUNCTION__, msg, con_conn_id);
	}

	//if connect message came in
	else if (strstr(msg.c_str(), "CONNECT") != NULL){ 		//length of the string == 4
		char *tk = strtok_r((char*)msg.c_str(),",", &tok_ptr2);
		if (tk != NULL){
			con_conn_id = atoi(tk);
		}
		if (con_conn_id >=0 && con_conn_id < MAX_NO_OF_AT_CLIENTS) {
			openClient(con_conn_id);
		}
		AT_resp_type = AT_R_CONNECT;
		DEBUG_ATC("[%s::%s] CONNECT found!!: %s, connection id: %d",
				name, __FUNCTION__, msg, con_conn_id);
	}

	else if (isMsgBleConn(msg.c_str())){
		AT_resp_type = AT_R_BLE_CONN;
	}

	else if (strstr(msg.c_str(), "CLOSED") != NULL){ 		//length of the string == 4
		char *tk = strtok_r((char*)msg.c_str(),",", &tok_ptr2);
		int client_id;
		if (tk != NULL){
			client_id = atoi(tk);
			//				conn_id = atoi(tk);
		}
		if (client_id >=0 && client_id < 8) {
			client[client_id].close();
		}
		AT_resp_type = AT_R_CLOSED;
		DEBUG_ATC("[%s::%s] CLOSED found!!: %s, connection id: %d",
				name, __FUNCTION__, msg, client_id);
	}

	else if (msg == "AT"){
		AT_resp_type = AT_R_AT_ECHO;
		DEBUG_ATC("[%s::%s] AT echo found: %s",
				name, __FUNCTION__, msg);
	}

	else if (strstr(msg.c_str(), "+BLEADDR:") != NULL)
	{
		AT_resp_type = AT_R_BLE_ADDR;
		addressBufferLen = parseAddressData(msg, addressBuffer, sizeof(addressBuffer));
		DEBUG_ATC("[%s::%s] AT ble address received: %s, addr: %s, len: %d",
						name, __FUNCTION__, msg, addressBuffer, addressBufferLen);
	}

	else {
		/**XXX: 3 managing broken ipd msgs
		 * if msg is not understood and an ipd message has been received two hours before
		 * */
		ipd_duration = eat_get_duration_ms(ipd_last_time);

		if (ipd_duration < IPD_TIMEOUT){
			AT_resp_type = AT_R_IPD;
			ipd_buff_temp<<msg;
			out_str = ipd_buff_temp.str();
			ipd_buff_temp.str("");
		}
		DEBUG_ATC("[%s::%s] can't find msg type, last IPD DURATION:%d",
				name, __FUNCTION__, ipd_duration);
		DEBUG_ATC("[%s::%s] msgs(this): %s",
				name, __FUNCTION__, msg);
		DEBUG_ATC("[%s::%s] msgs(combined): %s",
				name, __FUNCTION__, out_str);
	}

	return AT_resp_type;
}



string ATprotocol::getIncomingData(string r_msg, u32* msgLen, u32* conn_id){
	std::string msg_;
	if(parseWifiAtMsg(r_msg, msg_, msgLen, conn_id)) return msg_;
	DEBUG_ATC("[%s] CAN'T find wifi msg ", __FUNCTION__);

	if(parseBleAtMsg(r_msg, msg_, msgLen, conn_id)) return msg_;
	DEBUG_ATC("[%s] CAN'T find BLE msg either", __FUNCTION__);
	return "";
}

bool ATprotocol::parseWifiAtMsg(string r_msg, std::string& msg_out, u32* msgLen, u32* conn_id)
{
	if (strstr(r_msg.c_str(), "+IPD") == NULL) return false;

	int index_1;
	int index_2;
	int index_3;
	index_1 = r_msg.find(",", 0);			//find "," starting from index = 0
	index_2 = r_msg.find(",", index_1+1);	//start from the previous index
	index_3 = r_msg.find(":", index_2+1);

	string conn_id_str = r_msg.substr(index_1+1, index_2 - index_1 - 1 );
	string raw_msg_len = r_msg.substr(index_2+1, index_3 - index_2 - 1 );

	*conn_id = atoi(conn_id_str.c_str());
	*msgLen = atoi(raw_msg_len.c_str());
	msg_out = r_msg.substr(index_3+1);

	DEBUG_ATC("[%s::%s] ind1: %d, ind2: %d, ind3: %d, id: %d, msg_len: %d, parsed msg len: %d, prsed msg: %s",
			name, __FUNCTION__, index_1, index_2, index_3, *conn_id, *msgLen, msg_out.length(), msg_out);


	if (*conn_id <0 || *conn_id >= MAX_NO_OF_AT_CLIENTS) return false;
	new_client = &client[*conn_id];
	new_client->open(*conn_id);

	return true;
}

bool ATprotocol::parseBleAtMsg(string r_msg, std::string& msg_out, u32* msgLen, u32* conn_id)
{
	if (strstr(r_msg.c_str(), "+WRITE:") == NULL) return false; //+WRITE:1,1,5,,7,jgghh

	BleMsgParser parseBleMsg(r_msg.c_str());

	parseBleMsg.tokenize();

	/** Get message payload to verify it */
	std::string msg_verify = parseBleMsg.getMsgPayload();
	*msgLen = parseBleMsg.getMsglen();
	*conn_id = parseBleMsg.getConnid();

	DEBUG_ATC("[%s::%s]\"+WRITE:\" found!! con_id: %d, msg len: %d, rx len: %d, msg: %s ",
			name, __FUNCTION__, *conn_id, msg_verify.length(), *msgLen, msg_verify.c_str());

	if (msg_verify.length() != *msgLen ) return false;

	if (*conn_id <0 || *conn_id >= MAX_NO_OF_AT_CLIENTS) return false;

	new_client = &client_ble[*conn_id];
	new_client->open(*conn_id);
	new_client->servIndex = parseBleMsg.getServid();
	new_client->readXticsIndex = parseBleMsg.getXticsid();
	//TODO: add indication index to this later
	new_client->writeXticsIndex = 6;//TODO: DON'T EYE BALL THE WRITE xtics index

	DEBUG_ATC("[%s::%s]data len matches. IDs conn: %d, serv: %d",
			name, __FUNCTION__, *conn_id, new_client->servIndex);

	msg_out = msg_verify;
	return true;

}

int32_t ATprotocol::getMsgLen(string r_msg){
	int index_1 = r_msg.find(",", 0);			//find "," starting from index = 0
	int index_2 = r_msg.find(",", index_1+1);	//start from the previous index
	int index_3 = r_msg.find(":", index_2+1);

	string raw_msg_len = r_msg.substr(index_2+1, index_3-index_2-1);
	u32 msgLen = atoi(raw_msg_len.c_str());

	DEBUG_ATC("[%s::%s] ind1: %d, ind2: %d, ind3: %d, msg_len: %d, parsed msg len: %d, msg: %s",
			name, __FUNCTION__, index_1, index_2, index_3, msgLen);

	return msgLen+index_3+1; //1 accounts for ':'
}

//bool ATprotocol::parseAddressData(const char* r_msg, char* msg_out, int msg_size)
int ATprotocol::parseAddressData(std::string r_msg, char* msg_out, int msg_size)
{
	//find 1st " index in   +BLEADDR:"e8:db:84:1d:a2:fe"
//	char *e;
//
//
//	e = strchr(r_msg, '\"');
//	int index_1 = (int)(e - r_msg);
//	e = strchr(e+1, '\"');
//	int index_2 = (int)(e - r_msg);

    int index_1 = r_msg.find("\"", 0);          //find "," starting from index = 0
    int index_2 = r_msg.find("\"", index_1+1);  //start from the previous index
    std::string addr = r_msg.substr(index_1+1, index_2-index_1-1);

    return strlcpy(msg_out, addr.c_str(), msg_size);
}

/**
 * Adds to tk the index of the designator
 *
 * --- No error checking here, so be carefull
 */

/**  Adds to tk the index of the first character after the designator
 *  we add 1 to all designator index to have this
 *
 *  returns the length of incoming message, that would be 7 in the sample msg below
 *  		+WRITE:1,1,5,,7,jgghh
 *
*/





/*
 *
 */

int ATprotocol::send_cmd_and_get_resp(unsigned char* write_buf, unsigned int size, int duration_ms){
	int ret = 0;
	flush_cmd_input_buff();

	uart_write_fn(write_buf,size);
	ret = wait_for_cmd_resp(duration_ms);
	DEBUG_ATC("<%s> cmd avail?: %d, resp: %d, Sent: %s ",__FUNCTION__, cmd_available, ret, write_buf);
	return ret;
}

int ATprotocol::wait_for_cmd_resp(int duration_ms){
	int ret;
	int count = 1;
	int sleeptime = 5;
	count = duration_ms/sleeptime;

	while (!cmd_available && --count>0){
		eat_sleep(sleeptime);
	}

	ret = (count==0)? -1 : 0;	//if data came in before the time expires

	DEBUG_ATC("<%s> waiting return : %d, cmd_available: %d", __FUNCTION__, ret, cmd_available);
	if(ret == 0)
	{
		ret = read_from_cmd_buff();
	}
	return ret;
}

uint8_t bufff[1024]={0};
void ATprotocol::thread_function(){
	//get the buffer, put the
	memset(bufff, 0, sizeof(bufff));

	if (wifi_uart_obj==0) return;

	int ret = 0;
	ret = wifi_uart_obj->availData();
	if ( ret > 0) {
		//		eat_sleep(130); //wait for a max of 200 milli seconds in order to capture broken messages
		ret = wifi_uart_obj->readd((char*)bufff, sizeof(bufff));
		read_into_uart_stream((char*)bufff, ret);
	}


	if(data_available){
		string out_str;
		ret = process_received_msg(read_from_uart_stream(), out_str);
		if (ret == AT_R_IPD || ret == AT_R_WRITE){
			read_into_client_data_buff( (char*)out_str.c_str()  );
		}
		else if(!(ret == AT_R_NO_DATA || ret == AT_R_UNDEFINED)){
			read_into_cmd_buff(ret);
		}
	}
}


uint8_t AT_cmd_sem_name[] = "AT_CLIENT_MSG";
EatSemId_st AT_cmd_sem = eat_create_sem(AT_cmd_sem_name, 1);

void ATprotocol::read_into_cmd_buff(int cmd_int){

	while(!eat_sem_get(AT_cmd_sem, EAT_INFINITE_WAIT));

	if (!(cmd_stream << cmd_int << "\r")){	//if there is difficulty in writing into stream, flush first
		flush_cmd_input_buff();
		cmd_stream << cmd_int << "\r";
	}
	cmd_available = true;

	DEBUG_ATC("<%s> cmd: %d, str: %s, tellp: %d", __FUNCTION__, cmd_int, cmd_stream.str(), cmd_stream.tellp());

	eat_sem_put(AT_cmd_sem);
}

int ATprotocol::read_from_cmd_buff(){
	while(!eat_sem_get(AT_cmd_sem, EAT_INFINITE_WAIT));

	string out_str = "";
	if (getline(cmd_stream, out_str, '\r')){	//if getline is successful, there is still data
		cmd_available = true;
	}else{
		flush_cmd_input_buff();
	}
	DEBUG_ATC("<%s> cmd avail?: %d, str: %s, cmd(int): %d", __FUNCTION__, cmd_available, out_str, atoi(out_str.c_str()));

	eat_sem_put(AT_cmd_sem);
	return atoi(out_str.c_str());
}

void ATprotocol::flush_cmd_input_buff(){
	cmd_stream.clear();
	cmd_stream.str("");
	cmd_available = false;
}

uint8_t AT_client_sem_name[] = "AT_CLIENT_MSG";
EatSemId_st AT_client_sem = eat_create_sem(AT_client_sem_name, 1);

//COME BACK TO HANDLE ANY POSSIBLE ISSUE WITH READING INTO BUFFER as in cmd process
void ATprotocol::read_into_client_data_buff(char* raw_data){
	while(!eat_sem_get(AT_client_sem, EAT_INFINITE_WAIT));

	if(!(client_data_stream << raw_data << "\r")){//if there is any error while writing to
		flush_client_input_buff();
		client_data_stream << raw_data << "\r";
	}

	DEBUG_ATC("<%s> str: %s, tellp: %d", __FUNCTION__, raw_data, client_data_stream.tellp());
	client_data_available = true;

	eat_sem_put(AT_client_sem);
}

string ATprotocol::read_from_client_buff(Clients** local_cli){
	while(!eat_sem_get(AT_client_sem, EAT_INFINITE_WAIT));

	string out_str = "";
	u32 msgLen = 0;
	u32 _conn_id = 0;
	if (getline(client_data_stream, out_str, '\r')){	//if getline is successful, there is still data
		client_data_available = true;
		out_str = getIncomingData(out_str, &msgLen, &_conn_id);
		if (out_str != ""){
			*local_cli = new_client;
		}
//		if (_conn_id>=0 && _conn_id< MAX_NO_OF_AT_CLIENTS){
//			openClient(_conn_id);
//			*local_cli = new_client; 		//for evey openclient() call, newclient is been assigned the client pointer
//			DEBUG_ATC("<%s> GOT NEWCLIENT local cli addr 0x%x, new_client_addr 0x%x, id[%d], rx conn_id: %d", __FUNCTION__, *local_cli, new_client, new_client->conn_id, _conn_id);
//		}
	}else{
		flush_client_input_buff();
	}
	DEBUG_ATC("<%s> client avail?: %d, str: %s"
			, __FUNCTION__, client_data_available, out_str);

	eat_sem_put(AT_client_sem);
	return out_str;
}

void ATprotocol::flush_client_input_buff(){
	client_data_stream.clear();
	client_data_stream.str("");
	client_data_available = false;
}

ATprotocol::ATprotocol(){
	data_available 		= false;
	uart_input_stream;

	memset(uart_c_input_stream, 0, sizeof(uart_c_input_stream));
	memset(c_temp, 0, sizeof(c_temp));

	for (int i=0; i<(MAX_NO_OF_AT_CLIENTS); i++){
		client[i].conn_id = i;
		client_ble[i].conn_id = i;
	}

	null_client.conn = false;	//null client would be set to the default values
	null_client.session_id = 0;
	null_client.conn_id = 9;

	strcpy(name,"AT PROTOCOL");
}




Clients::Clients(){
	conn = false;
	session_id = 0;
	type = WIFI;
	conn_id = -1;	//these are just default values

	readXticsIndex = 0;
	writeXticsIndex = 0;
	servIndex = 0;
}

void Clients::close(){
	conn = false;
	session_id = 0;
}

void Clients::open(int con_id){
	conn = true;
	conn_id = con_id;
	//think about assigning session id  to zero here
}

Clients_ble::Clients_ble()
{
	type = BLE;
}

Clients_ble::~Clients_ble()
{
	;
}

bool ATprotocol::openClient(int con_id, char* uuid, Clients::Client_type client_ty ){
	if (con_id <0 || con_id >= MAX_NO_OF_AT_CLIENTS) return false;

	new_client = (client_ty == Clients::BLE)? &client_ble[con_id] : &client[con_id];

	new_client->open(con_id);

	if (uuid != NULL ) STRLCPY_(new_client->uuid, uuid);

	return true;
}

void ATprotocol::closeClientsExcept(int con_id){
	for (int i=0; i<MAX_NO_OF_AT_CLIENTS; i++){
		if (i == con_id) continue;
		client[i].close();
	}
}







/***
 *
 */

BleMsgParser::BleMsgParser(const char* msg_str)
{
	msgStr = &msg_str;
//	tk[10];
}

BleMsgParser::~BleMsgParser()
{
}

int BleMsgParser::setMsgStr(const char* msg_in)
{
	msgStr = &msg_in;
	return 0;
}

int BleMsgParser::tokenize()
{
	std::string m = *msgStr;
//	tk[0] = 1 + m.find(":", 0);           //find ":" starting from index = 0
	tk[0] = m.find(":", 0);           //find ":" starting from index = 0
	tk[1] = m.find(",", tk[0] + 1);   //start from the previous index
	tk[2] = m.find(",", tk[1] + 1);
	tk[3] = m.find(",", tk[2] + 1);
	tk[4] = m.find(",", tk[3] + 1);
	tk[5] = m.find(",", tk[4] + 1);
	return 0;
}
//*  		+WRITE:1,1,5,,7,jgghh
int BleMsgParser::readNumAtTkn(int tk_index)
{
	std::string m = *msgStr;

	int16_t tkn_ind_1 = tk[tk_index] + 1; /**< index of char after token " ,7"  */

	std::string raw_num = m.substr(tkn_ind_1, tk[ tk_index + 1] - tkn_ind_1); //

	uint32_t num = atoi(raw_num.c_str());

	DEBUG_ATC("\n[%s] tk ind: %d, ind + 1: %d, NUMBER: %d"
			,__FUNCTION__, tk_index, tkn_ind_1, num);

	return num; //1 accounts for ':'
}

int BleMsgParser::getMsglen()
{
	return readNumAtTkn(4);
}

int BleMsgParser::getConnid(){
	return readNumAtTkn(0);
}

int BleMsgParser::getServid(){
	return readNumAtTkn(1);
}

int BleMsgParser::getXticsid()
{
	return readNumAtTkn(2);
}

int BleMsgParser::getIndictnId()
{
	return readNumAtTkn(3);
}

std::string BleMsgParser::getMsgPayload(char* msg_out, int size)
{

	int16_t tkn_5_1 = tk[5]+1;

	std::string m = *msgStr;

	std::string msg = m.substr(tkn_5_1);

	DEBUG_ATC("\n[%s] len: %d, msg: %s", __FUNCTION__, msg.length(), msg.c_str());

	if (msg_out != NULL) strlcpy(msg_out, msg.c_str(), size-1);

	return msg;
}
