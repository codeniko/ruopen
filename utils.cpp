list<string> providerEmails = { 
	"@txt.att.net",//AT&T
	"@vtext.com",//Verizon
	"@tmomail.net",//T-Mobile
	"@messaging.sprintpcs.com",//Sprint PCS
	"@messaging.nextel.com",//Sprint Nextel
	"@vmobl.com",//Virgin Mobile USA
	"@email.uscc.net",//US Cellular
	"@mymetropcs.com",//MetroPCS
	"@mobile.celloneusa.com",//Cellular One
	"@myhelio.com",//Helio
	"@myboostmobile.com",//Boost Mobile
	"@message.alltel.com",//Alltel
	"@sms.bluecell.com",//Bluegrass Cellular
	"@cwemail.com",//Centennial
	"@qwestmp.com",//Qwest
};

char payload_text[][70] = {
	"To: RUopen@ymail.com",
	"Bcc: RUopen@ymail.com (RUopen)\r\n",
	"\r\n", /* empty line to divide headers from body, see RFC5322 */
	"\r\n"
};

string semesterCodeToString(string code)
{
	char c = code[0];
	switch(c) {
		case '1': return string("Spring ").append(code.substr(1,4));
		case '0': return string("Winter ").append(code.substr(1,4));
		case '9': return string("Fall ").append(code.substr(1,4));
		case '7': return string("Summer ").append(code.substr(1,4));
		default: 
				  cerr << "ERROR: Invalid semester" << endl;
				  exit(1);
	}
}

string semesterStringToCode(string str)
{
	list<string> tokens;
	boost::regex_split(std::back_inserter(tokens), str);
	if (tokens.size() != 2)
		return "NULL";
	str = *tokens.begin();
	transform(str.begin(), str.end(), str.begin(), ::tolower);
	tokens.pop_front();
	string year = *tokens.begin();
	if (str == "spring") return string("1") + year;
	else if (str == "winter") return string("0") + year;
	else if (str == "fall") return string("9") + year;
	else if (str == "summer") return string("7") + year;
	else return "NULL";
}

//Used for Email libcurl
struct upload_status {
	int lines_read;
};

//Used for Email libcurl
size_t payload_source(void *ptr, size_t size, size_t nmemb, void *userp)
{
	upload_status *upload_ctx = (upload_status *)userp;
	const char *data;

	if((size == 0) || (nmemb == 0) || ((size*nmemb) < 1)) {
		return 0;
	}

	data = payload_text[upload_ctx->lines_read];

	if(data) {
		size_t len = strlen(data);
		memcpy(ptr, data, len);
		upload_ctx->lines_read++;

		return len;
	}

	return 0;
}

void debug()
{
	ofstream file;
	file.open("response.html");
	file << curl.response;
	file.close();
	cout << "Response code: " << curl.res << endl;
	cout << "Response length: " << curl.respLen << endl;
	cout << "Response header: \n" << curl.responseHeader << endl;
}
