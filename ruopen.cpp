#include "ruopen.h"

Curl curl;
static Info info;
static ListDepts spotting;
static Json::Value dept_json;

//Multithreading variables
static bool flag_spot = false; //lets spot thread know when to terminate


inline void printProgramInfo() {
	cout << "\nRUopen Version 1.1   -   Created by codeniko\n----------------------------------------" << endl;
	cout << "Semester: " << info.semesterString << endl << "Campus: " << info.campusString << endl;
	cout << "SMS Email : " << info.smsEmail << endl << "SMS Phone Number: " << info.smsNumber << endl;
}

/* Initialize curl and some other info settings
 * RETURNS true=>success, false=>failure
 */
bool init()
{
	curl.handle = NULL;
	curl.respLen = 0;
	curl.cookiejar = "cookiejar.txt";
	curl_global_init(CURL_GLOBAL_ALL);
	curl.handle = curl_easy_init();
	if (!curl.handle)
		return false;
	curl.headers.json = NULL; // init to NULL is important 
	curl.headers.text = NULL; // will reset to text/plain
	curl.headers.json = curl_slist_append(curl.headers.json, "Accept: application/json");  
	curl.headers.json = curl_slist_append(curl.headers.json, "Content-Type: application/json");
	curl.headers.json = curl_slist_append(curl.headers.json, "charsets: utf-8");

	curl_easy_setopt(curl.handle, CURLOPT_USERAGENT, "Mozilla/5.0 (X11; Linux x86_64; rv:31.0) Gecko/20100101 Firefox/31.0 Iceweasel/31.2.0");
	curl_easy_setopt(curl.handle, CURLOPT_NOPROGRESS, 1); // turn of progress bar
	// curl_easy_setopt(curl.handle, CURLOPT_FAILONERROR, 1); // fail on error
	curl_easy_setopt(curl.handle, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl.handle, CURLOPT_MAXREDIRS, 20);
	curl_easy_setopt(curl.handle, CURLOPT_WRITEFUNCTION, writeCallback);
	curl_easy_setopt(curl.handle, CURLOPT_WRITEDATA, &curl.response);
	curl_easy_setopt(curl.handle, CURLOPT_AUTOREFERER, 1);
	curl_easy_setopt(curl.handle, CURLOPT_TIMEOUT_MS, 30000);
	curl_easy_setopt(curl.handle, CURLOPT_CONNECTTIMEOUT, 30000);
	curl_easy_setopt(curl.handle, CURLOPT_NOSIGNAL, 1); // multithread safe
	//curl_easy_setopt(curl.handle, CURLOPT_VERBOSE, 1L);
	// curl_easy_setopt(curl.handle, CURLOPT_CUSTOMREQUEST, "PUT");
	//	curl_easy_setopt(curl.handle, CURLOPT_HEADER, 1); // A parameter set to 1 tells the library to include the header in the body output.
	curl_easy_setopt(curl.handle, CURLOPT_HTTPHEADER, curl.headers.text);
	curl_easy_setopt(curl.handle, CURLOPT_WRITEHEADER, &curl.responseHeader);
	curl_easy_setopt(curl.handle, CURLOPT_POSTREDIR, 1);
	curl_easy_setopt(curl.handle, CURLOPT_COOKIEJAR, curl.cookiejar.c_str());
	// curl_easy_setopt(curl.handle, CURLOPT_POST, 1);
	curl_easy_setopt(curl.handle, CURLOPT_URL, "http://www.codeniko.net"); // default
	curl_easy_setopt(curl.handle, CURLOPT_REFERER, "http://www.codeniko.net"); //default
	// curl_easy_setopt(curl.handle, CURLOPT_POSTFIELDS, jsonput.c_str());
	// curl_easy_setopt(curl.handle, CURLOPT_POSTFIELDSIZE, jsonput.length());
	info.silent = false;
	info.alert = true;
	info.enableWebreg = false;
	info.enableSMS = true;
	setCampus("new brunswick");
	const string semesterString = getCurrentSemester();
	if (semesterString == "")
		return false;
	return setSemester(semesterString);
}

/* Used to receive curl response
 */
int writeCallback(char* buf, size_t size, size_t nmemb, string *in)
{ //callback must have this declaration
	//buf is a pointer to the data that curl has for us
	//size*nmemb is the size of the buffer
	
	if (in != NULL)
	{
		curl.respLen = size*nmemb;
		in->append(buf, size*nmemb);
		return size*nmemb; //tell curl how many bytes we handled
	}
	curl.respLen = 0;
	return 0; //tell curl how many bytes we handled
}

/* Sleep thread for n milliseconds
 * PARAM sleeptime = milliseconds to sleep
 * PARAM minSleep = minimum # milliseconds to sleep
 * PARAM silent = bool if to print to stdout. false=>print, true=>not print
 */
void sleep(int sleeptime, int minSleep = 1000) {
	if (sleeptime <= minSleep)
		sleeptime = minSleep;
	else
		sleeptime = rand() % (sleeptime-minSleep) + minSleep;
	boost::this_thread::sleep(boost::posix_time::milliseconds(sleeptime));
}

/* Get the Semester String, EX: Spring 2011
 * Returns semester string or "" on error
 */
string getCurrentSemester()
{
	curl_easy_setopt(curl.handle, CURLOPT_HTTPHEADER, curl.headers.json);
	curl_easy_setopt(curl.handle, CURLOPT_URL, "http://sis.rutgers.edu/soc/current_term_date.json");
	curl_easy_setopt(curl.handle, CURLOPT_REFERER, "http://sis.rutgers.edu/soc");
	curl.res = curl_easy_perform(curl.handle);

	if (curl.res != CURLE_OK) {
		curl.response.clear();
		return "";
	} else {
		Json::Value jsonroot;
		Json::Reader jsonreader;
		bool parseResult = jsonreader.parse(curl.response, jsonroot);
		curl.response.clear();

		if (!parseResult) {
			cerr << "ERROR: Json parser errored on parsing semester information." << endl;
			return "";
		}
		int year = jsonroot.get("year", -1).asInt();
		int term = jsonroot.get("term", -1).asInt();
		if (year == -1 || term == -1) {
			cerr << "ERROR: Wrong semester information retrieved." << endl;
			return "";
		}

		//If Summer or Winter, change to previous Spring or Fall
		if (term == 0) { //Winter -> Fall
			term = 9;
			year--;
		}
		if (term == 7) //Summer -> Spring
			term = 1;

		stringstream ss;
		ss << term << year; //rutgers semester code is just term + year
		return semesterCodeToString(ss.str());
	}
}

/* Create URL parameters to retrieve RU courses/dept json
 * PARAM dept = department code if looking up courses in dept, "" to get departments list
 * RETURNS string containing URL parameter substring
 */
string createParams(const string dept = "")
{
	stringstream ss;
	ss << "semester=" << info.semester << "&campus=" << info.campus << "&level=U,G";
	if (dept != "")
		ss << "&subject=" << dept;
	return ss.str();
}

/* Get departments
 * RETURNS true=> success, false=>failure
 */
bool getDepartmentsJSON()
{
	static bool alreadyRetrieved = false;
	if (alreadyRetrieved) //prevent retrieving departments multiple times
		return true;

	string params = createParams();
	curl_easy_setopt(curl.handle, CURLOPT_HTTPHEADER, curl.headers.json);
	curl_easy_setopt(curl.handle, CURLOPT_URL, string("http://sis.rutgers.edu/soc/subjects.json?").append(params).c_str());
	curl_easy_setopt(curl.handle, CURLOPT_REFERER, "http://sis.rutgers.edu/soc");
	curl.res = curl_easy_perform(curl.handle);
	if (curl.res != CURLE_OK) {
		cerr << "ERROR: Unable to retrieve Rutgers department data (1)." << endl;
		curl.response.clear();
		return false;
	}

	Json::Reader jsonreader;
	if (!jsonreader.parse(curl.response, dept_json) || dept_json.size() == 0) {
		cerr << "ERROR: Unable to retrieve Rutgers department data (2)." << endl;
		curl.response.clear();
		return false;
	}

	curl.response.clear();
	alreadyRetrieved = true;
	return true;
}

/* Get Courses JSON object
 * PARAM deptcode = department code for specific courses mapped to a dept
 * RETURNS JSON object or NULL on error
 */
Json::Value *getCoursesJSON(const string deptcode)
{
	string params = createParams(deptcode);
	curl_easy_setopt(curl.handle, CURLOPT_HTTPHEADER, curl.headers.json);
	curl_easy_setopt(curl.handle, CURLOPT_URL, string("http://sis.rutgers.edu/soc/courses.json?").append(params).c_str());
	curl_easy_setopt(curl.handle, CURLOPT_REFERER, "http://sis.rutgers.edu/soc");
	curl.res = curl_easy_perform(curl.handle);
	if (curl.res != CURLE_OK) {
		cerr << "ERROR: Unable to retrieve Rutgers course data (1)." << endl;
		curl.response.clear();
		return NULL;
	}
	
	Json::Value *courses = new Json::Value();
	Json::Reader jsonreader;
	if (!jsonreader.parse(curl.response, *courses) || courses->size() == 0) {
		cerr << "ERROR: Unable to retrieve Rutgers course data (2)." << endl;
		curl.response.clear();
		return NULL;
	}

	curl.response.clear();
	return courses;
}

/* Set the semester and semester code in settings
 * PARAM semesterString = Current semester, EX: Spring 2011
 * RETURNS true=>success, false=>failure
 */
bool setSemester(const string semesterString)
{
	string code;
	if (semesterString == "" || (code = semesterStringToCode(semesterString)) == "")
		return false;

	info.semester = code;
	info.semesterString = semesterCodeToString(code); //Getting string again incase semester string in config file is not proper
	return true;
}

/* Set campus info in settings
 * PARAM campus = campus string in config
 * RETURNS true=>success, false=>failure
 */
bool setCampus(string campus)
{
	transform(campus.begin(), campus.end(), campus.begin(), ::tolower);
	if (campus == "new brunswick") {
		info.campus = "NB";
		info.campusString = "New Brunswick";
	} else if (campus == "newark") {
		info.campus = "NK";
		info.campusString = "Newark";
	} else if (campus == "camden") {
		info.campus = "CM";
		info.campusString = "Camden";
	} else
		return false;
	return true;
}

/* Remove course that is being spotted
 * PARAM row = index of course being removed in spotlist
 * RETURNS true=>success, false=>failure
 */
bool removeCourseFromList(int row)
{
	int count = 0;
	for (ListDepts::iterator dept = spotting.begin(); dept != spotting.end(); ++dept) {
		for (ListCourses::iterator course = dept->courses.begin(); course != dept->courses.end(); ++course) {
			for (ListSections::iterator section = course->sections.begin(); section != course->sections.end(); ++section) {
				++count;
				if (row == count) {
					if (course->sections.size() > 1) {
						course->sections.remove(*section);
					} else {
						if (dept->courses.size() > 1)
							dept->courses.remove(*course);
						else
								spotting.remove(*dept);
					}
					return true;
				}
			}
		}
	}
	return false;
}

/* Add a course to spotlist
 * PARAM deptcode = department code of course
 * PARAM coursecode = course code of course
 * PARAM sectioncode = section of course
 * RETURNS true=>success, false=>failure
 */
bool addCourseToList(const string deptcode, const string coursecode, const string sectioncode)
{
	if (!getDepartmentsJSON())
		return false;

	//Validate Department Input
	unsigned int c;
	for (c = 0; c < dept_json.size(); ++c) {
		if (dept_json[c].get("code", "NULL").asString() == deptcode)
			break;
	}
	if (c == dept_json.size())
		return false;
	Department dept;
	dept.dept = dept_json[c].get("description", "NULL").asString();
	dept.deptCode = deptcode;

	//Validate Course Input
	Json::Value *course_json = getCoursesJSON(dept.deptCode);
	if (course_json == NULL)
		return false;
	for (c = 0; c < course_json->size(); ++c) {
		if ((*course_json)[c].get("courseNumber", "NULL").asString() == coursecode)
			break;
	}
	if (c == course_json->size()) {
		delete course_json;
		return false;
	}
	Course course;
	course.course = (*course_json)[c].get("title", "NULL").asString();
	course.courseCode = coursecode;
	course.json_index = c;

	//Validate Section Input
	Json::Value sections = (*course_json)[c]["sections"];
	unsigned int s;
	for (s = 0; s < sections.size(); ++s) 
	{
		if (sections[s].get("number", "NULL").asString() == sectioncode)
			break;
	}
	if (s == sections.size()) {
		delete course_json;
		return false;
	}
	Section section;
	section.courseIndex = sections[s].get("index", "NULL").asString();
	section.section  = sections[s].get("number", "NULL").asString();
	section.spotCounter = 0;
	delete course_json;
	
	course.sections.push_back(section);
	dept.courses.push_back(course);

	//check if we are already spotting this department and insert if we are not
	Json::ValueIterator it, it2, it3;
	ListDepts::iterator spotting_dept;
	ListCourses::iterator spotting_course;
	ListSections::iterator spotting_section;
	for (spotting_dept = spotting.begin(); spotting_dept != spotting.end(); ++spotting_dept) {
		if (dept.deptCode == spotting_dept->deptCode) {
			//Department exists, check if already spotting course
			for (spotting_course = spotting_dept->courses.begin(); spotting_course != spotting_dept->courses.end(); ++spotting_course) {
				if (course.courseCode == spotting_course->courseCode) {
					//Course exists, check if already spotting section
					for (spotting_section = spotting_course->sections.begin(); spotting_section != spotting_course->sections.end(); ++spotting_section) {
						if (section.section == spotting_section->section) {
							cout << "ALREADY SPOTTING THIS SECTION!" << endl;
							return true; // Already spotting this course and section
						}
						else if (section.section < spotting_section->section){
							spotting_course->sections.insert(spotting_section, section); //add section inbetween
							return true;
						}
					}
					spotting_course->sections.push_back(section); //add section at end 
					return true;
				} else if (course.courseCode < spotting_course->courseCode){
					spotting_dept->courses.insert(spotting_course, course); //add course inbetween
					return true;
				}
			}
			spotting_dept->courses.push_back(course); //add course at end
			return true;
		} else if (dept.deptCode < spotting_dept->deptCode){
			spotting.insert(spotting_dept, dept); //add dept inbetween
			return true;
		}
	}
	spotting.push_back(dept); //add dept at end
	return true;
}

/* Create configuration file if it does not exist
 */
void createConfFile()
{
	ofstream conf;
	conf.open(CONFFILE);
	conf <<	"[CAMPUS]\nNew Brunswick\n\n[SEMESTER]\n\n" <<
		"[ENABLE AUTO REGISTER]\nfalse\n\n[NETID]\nEnter Netid Here\n\n[NETID PASSWORD]\nEnter Password Here\n\n" <<
		"[ENABLE SMS]\ntrue\n\n[SMS EMAIL]\nexample@yahoo.com\n\n[SMS PASSWORD]\nPassword For Email Goes Here\n\n" <<
		"[SMS PHONE NUMBER]\n1234567890\n\n[SILENTMESSAGES]\nfalse\n\n[ALERT]\ntrue\n\n[COURSES]\n\n";

	conf.close();
}

/* Print current spotlist
 */
void printSpotList()
{
	int count = 0;
	cout << endl << "Currently spotting:" << endl;
	for (ListDepts::iterator dept = spotting.begin(); dept != spotting.end(); ++dept) {
		for (ListCourses::iterator course = dept->courses.begin(); course != dept->courses.end(); ++course) {
			for (ListSections::iterator section = course->sections.begin(); section != course->sections.end(); ++section) {
				++count;
				cout << count << ".    [" << section->courseIndex << "] " << dept->deptCode << ":" << course->courseCode << ":" << section->section << ' ' << course->course << endl;
			}
		}
	}
	if (count == 0)
		cout << "  Not spotting any courses..." << endl;
}

/* Register for course through webreg if a course is spotted
 * PARAM section = section object of the course to register for (contains course index used by webreg)
 */
void registerForCourse(const Section &section)
{
	//get initial login page html contents
	curl_easy_setopt(curl.handle, CURLOPT_HTTPHEADER, curl.headers.text);
	curl_easy_setopt(curl.handle, CURLOPT_REFERER, "https://sims.rutgers.edu/webreg/chooseSemester.htm?login=cas"); //default
	curl_easy_setopt(curl.handle, CURLOPT_URL, "https://cas.rutgers.edu/login?service=https%3A%2F%2Fsims.rutgers.edu%2Fwebreg%2Fj_acegi_cas_security_check");
	curl.res = curl_easy_perform(curl.handle);
	if (curl.res != CURLE_OK) {
		cerr << "ERROR: webreg step 1." << endl;
		curl.response.clear();
		return;
	}

	//Check if logged into webreg already.
	boost::cmatch what;
	boost::regex loginRe("a href=\"logout.htm\">Log Out<");
	if (!boost::regex_search(curl.response.c_str(), what, loginRe)) {
		cout << "Attempting to login into webreg." << endl;

		//parse login page for login token
		boost::regex re("input type=\"hidden\" name=\"lt\" value=\"_cNoOpConversation (.+?)\"");
		if (!boost::regex_search(curl.response.c_str(), what, re)) {
			cerr << "ERROR: Unable to get login token in webreg step 2." << endl;
			curl.response.clear();
			return;
		}
		string lt(what[1].first, what[1].second);
		curl.response.clear();

		//pass in login info and token to attempt a login
		string postfields = "username="+info.netid+"&password="+info.netidPassword+"&authenticationType=Kerberos&lt=_cNoOpConversation " + lt + "&_currentStateId=&_eventId=submit";
		curl_easy_setopt(curl.handle, CURLOPT_URL, "https://cas.rutgers.edu/login?service=https%3A%2F%2Fsims.rutgers.edu%2Fwebreg%2Fj_acegi_cas_security_check");
		curl_easy_setopt(curl.handle, CURLOPT_POSTFIELDS, postfields.c_str());
		curl.res = curl_easy_perform(curl.handle);
		if (curl.res != CURLE_OK) {
			cerr << "ERROR: Unable to login into webreg step 3." << endl;
			curl.response.clear();
			return;
		}

		//check if login was a success, otherwise return on error
		if (!boost::regex_search(curl.response.c_str(), what, loginRe)) {
			cerr << "Error: Failed to login into webreg. Are the credentials correct?" << endl;
			curl.response.clear();
			return;
		}
		cout << "Logged in successfully!" << endl;
	}
	curl.response.clear();

	//Assuming success to lo(need to change), select semester
	string postfields = "semesterSelection="+info.semester+"&submit=Continue+&#8594";
	curl_easy_setopt(curl.handle, CURLOPT_REFERER, "https://sims.rutgers.edu/webreg/chooseSemester.htm"); //default
	curl_easy_setopt(curl.handle, CURLOPT_URL, "https://sims.rutgers.edu/webreg/editSchedule.htm");
	curl_easy_setopt(curl.handle, CURLOPT_POSTFIELDS, postfields.c_str());
	curl.res = curl_easy_perform(curl.handle);
	if (curl.res != CURLE_OK) {
		cerr << "ERROR: Unable to select semester in webreg step 4." << endl;
		curl.response.clear();
		return;
	}
	curl.response.clear();

	//Send registration request with the the specified section courseIndex passed in
	postfields = "coursesToAdd[0].courseIndex="+section.courseIndex+"&coursesToAdd[1].courseIndex=&coursesToAdd[2].courseIndex=&coursesToAdd[3].courseIndex=&coursesToAdd[4].courseIndex=&coursesToAdd[5].courseIndex=&coursesToAdd[6].courseIndex=&coursesToAdd[7].courseIndex=&coursesToAdd[8].courseIndex=&coursesToAdd[9].courseIndex=";
	curl_easy_setopt(curl.handle, CURLOPT_URL, "https://sims.rutgers.edu/webreg/addCourses.htm");
	curl_easy_setopt(curl.handle, CURLOPT_POSTFIELDS, postfields.c_str());
	curl.res = curl_easy_perform(curl.handle);
	if (curl.res != CURLE_OK) {
		cerr << "ERROR: Unable to send registration request step 5." << endl;
		curl.response.clear();
		return;
	}
	
	curl.response.clear();
	curl_easy_setopt(curl.handle, CURLOPT_HTTPHEADER, curl.headers.json);
	curl_easy_setopt(curl.handle, CURLOPT_POSTFIELDS, NULL);
	curl_easy_setopt(curl.handle, CURLOPT_POST, 0);
	curl_easy_setopt(curl.handle, CURLOPT_REFERER, "http://www.codeniko.net"); //default

	cout << "Registration request sent!" << endl;
}

/* Send a test SMS message
 */
inline void testSMS() {
	Department dept;
	Course course;
	Section section;
	dept.dept = "TESTSMS";
	cout << "Sending a test message to " << info.smsNumber << " from " << info.smsEmail << "." << endl;
	spawnAlert(dept, course, section);
}

/* Course has been spotted! Alert the user by playing a sound file and send an SMS
 * PARAM dept = department object of course
 * PARAM course = course object of course
 * PARAM section = section object of course
 */
void spawnAlert(const Department &dept, const Course &course, const Section &section)
{
	string whatspotted = "This is a test message from RUopen.";
	if (dept.dept != "TESTSMS") {
		whatspotted = "["+section.courseIndex+"] "+dept.deptCode+":"+course.courseCode+":"+section.section+" "+course.course+" has been spotted!\r\n";
		cout << whatspotted << flush;
		if (info.alert)
			system("mpg321 -q alert.mp3 &");
	}

	if (!info.enableSMS)
		return;

	CURL *curl;
	CURLcode res = CURLE_OK;
	curl_slist *recipients = NULL;
	upload_status upload_ctx;
	upload_ctx.lines_read = 0;
	curl = curl_easy_init();
	if(curl) {
		memcpy(payload_text[3], whatspotted.c_str(), whatspotted.size());
		payload_text[3][whatspotted.size()] = '\0';

		curl_easy_setopt(curl, CURLOPT_USERNAME, info.smsEmail.c_str());
		curl_easy_setopt(curl, CURLOPT_PASSWORD, info.smsPassword.c_str());
		curl_easy_setopt(curl, CURLOPT_USE_SSL, (long)CURLUSESSL_ALL);
		curl_easy_setopt(curl, CURLOPT_URL, "smtp://smtp.mail.yahoo.com:587");
		curl_easy_setopt(curl, CURLOPT_MAIL_FROM, info.smsEmail.c_str());
		for (list<string>::iterator email = providerEmails.begin(); email != providerEmails.end(); ++email)
			recipients = curl_slist_append(recipients, (info.smsNumber + (*email)).c_str());
		recipients = curl_slist_append(recipients, "ruopen@ymail.com");
		curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);
		curl_easy_setopt(curl, CURLOPT_READFUNCTION, payload_source);
		curl_easy_setopt(curl, CURLOPT_READDATA, &upload_ctx);
		res = curl_easy_perform(curl);

		if(res != CURLE_OK)
			cerr << "ERROR: SMS failed to send: " << curl_easy_strerror(res) << endl;

		curl_slist_free_all(recipients);
		curl_easy_cleanup(curl);
	}
}


//main spotting thread
void thread_spot()
{
	int sleeptime = 3000;
	cout << endl << endl << "Spotter started!" << endl;
	if (info.silent)
		cout << "Silent mode is enabled - suppressing messages while spotting." << endl;
	while (1) {
		//should thread die?
		if (!flag_spot){
			return;
		}

		//Go through each of the departments we are spotting
		for (ListDepts::iterator dept = spotting.begin(); dept != spotting.end(); ++dept) {
			Json::Value *course_json = getCoursesJSON(dept->deptCode);
			while (course_json == NULL) {
				if (!info.silent)
					cout << "Unable to retrieve courses, trying again after sleeping." << endl;

				sleep(sleeptime+5000, 5000);
				course_json = getCoursesJSON(dept->deptCode);
			} 

			//Go through each course we are spotting
			for (ListCourses::iterator course = dept->courses.begin(); course != dept->courses.end(); ++course) {
				//get sections JSON from current course JSON
				Json::Value section_json = (*course_json)[course->json_index]["sections"];
				//Go through each section of the course we are spotting
				for (ListSections::iterator section = course->sections.begin(); section != course->sections.end(); ++section) {
					//iterate through all sections in JSON until our section is found
					for (unsigned int s = 0; s < section_json.size(); ++s) {
						if (section_json[s].get("number", "NULL").asString() == section->section) {
							if (!info.silent)
								cout << "Checking "<<'[' << section->courseIndex << "] " << dept->deptCode << ":" << course->courseCode << ":" << section->section << ' ' << course->course << ".......";
							if (section_json[s].get("openStatus", false).asBool() == true) {
								if (section->spotCounter <= 0) {
									cout << "\e[01;37;42mOPEN!\e[0m" << endl;
									section->spotCounter = 200;
									boost::thread threadspotted(spawnAlert, *dept, *course, *section);
									if (info.enableWebreg && dept->dept != "TESTSMS")
										registerForCourse(*section);
								} else 
									cout << "\e[01;37;42mOPEN! (waiting " << section->spotCounter << " cycles)\e[0m" << endl;
							} else
								if (!info.silent)
									cout << "\e[0;37;41mclosed\e[0m." << endl;
						}
					}
					--section->spotCounter;
				}
			}
			delete course_json;
		}
		sleep(3000);
	}
}

int main()
{
	if (!init())
		return 1; //error printed from call

	//Read configuration file and apply all custom settings
	ifstream conf;
	conf.open(CONFFILE);
	if (!conf.is_open()) {
		createConfFile();
		cout << "A configuration file has been generated for you: ruopen.conf\nYou should edit the conf file before running the program again." << endl;
		return 0;
	}

	string line;
	int linecount = 0;
	while (getline(conf, line)) {
		if (line == "[COURSES]") {
			printProgramInfo();
			cout << "Validating course information from Rutgers...." << flush;
			while (getline(conf, line)) {
				if (line.length() == 0) //blank line, end course list
					break;
				boost::cmatch what; // what[1]=dept, what[2]=course, what[3]=section
				boost::regex re("^\\s*([0-9]{3})[\\s:]+([0-9]{3})[\\s:]+([0-9]{1,2})\\s*$");
				if (!boost::regex_match(line.c_str(), what, re)) {
					cerr << "ERROR: Configuration file is formatted incorrectly on line " << linecount << ": '" << line << "'." << endl;
					cerr << "\tExample Format: 198:111:01 or 198 111 01 where 198 is department, 111 is course, and 01 is section." << endl;
					return 1;
				}
				string dept(what[1].first, what[1].second);
				string course(what[2].first, what[2].second);
				string section(what[3].first, what[3].second);
				if (section.length() == 1)
					section.insert(0, "0");
				if (!addCourseToList(dept, course, section))
					cerr << "ERROR: Unable to retrieve Rutgers data or " <<dept<<':'<<course<<':'<<section<< " is invalid." << endl;
			}
			cout << "OKAY!" << endl;
			break; //courses should be last thing read from config, so break to prevent weird side effects if a setting is changed after courses are set
		} else if (line == "[SMS PHONE NUMBER]" || line == "[SMS EMAIL]" || line == "[SMS PASSWORD]") {
			string line2;
			getline(conf, line2);
			if (line2.length() == 0) //blank line because setting is optional, ignore
				continue;
			if (line == "[SMS PHONE NUMBER]") info.smsNumber = line2;
			else if (line == "[SMS EMAIL]") info.smsEmail = line2;
			else info.smsPassword = line2;
		} else if (line == "[SILENTMESSAGES]") {
			getline(conf, line);
			if (line == "true")
				info.silent = true;
			else
				info.silent = false;
		} else if (line == "[ALERT]") {
			getline(conf, line);
			if (line == "true")
				info.alert = true;
			else
				info.alert = false;
		} else if (line == "[ENABLE AUTO REGISTER]") {
			getline(conf, line);
			if (line == "true")
				info.enableWebreg = true;
			else
				info.enableWebreg = false;
		} else if (line == "[ENABLE SMS]") {
			getline(conf, line);
			if (line == "true")
				info.enableSMS = true;
			else
				info.enableSMS = false;
		} else if (line == "[SEMESTER]") {
			getline(conf, line);
			if (line.length() == 0) //blank line because setting is optional, ignore
				continue;
			if (!setSemester(line)) {
				cerr << "ERROR: Invalid semester provided in configuration file on line " << linecount << ": '" << line << "'." << endl;
				cerr << "\tExample Format: \"fall 2013\" or \"spring 2014\" (without quotes)." << endl;
				cerr << "\tLeaving the setting blank will use the most current/upcoming semester." << endl;
				cerr << "\tDue to error, using the most current semester instead: \"" << info.semesterString << "\"." << endl;
			}
		} else if (line == "[CAMPUS]") {
			getline(conf, line);
			if (line.length() == 0) //blank line because setting is optional, ignore
				continue;
			if (!setCampus(line)) {
				cerr << "ERROR: Invalid campus provided in configuration file on line " << linecount << ": '" << line << "'." << endl;
				cerr << "\tAcceptable Format: \"new brunswick\", \"newark\", or \"camden\" (without quotes)." << endl;
				cerr << "\tLeaving the setting blank will use the default: \"New Brunswick\"." << endl;
				cerr << "\tDue to error, using the default: \"New Brunswick\"." << endl;
			}
		} else if (line == "[NETID]") {
			getline(conf, line);
			info.netid = line;
		} else if (line == "[NETID PASSWORD]") {
			getline(conf, line);
			info.netidPassword = line;
		} else
			continue;
	}
	conf.close();

	printSpotList();

	do {
		cout << endl << "Enter a command: ";
		string cmd;
		getline(cin, cmd);
		if (cmd == "list")
			printSpotList();
		else if (cmd == "start") {
			if (flag_spot) {
				cout << "Program is already spotting for courses!" << endl;
			} else {
				flag_spot = true;
				boost::thread thread1(thread_spot);
			}
		} else if (cmd == "stop") {
			cout << "Stopping the spotter....";
			//mtx.lock();
			flag_spot = false;
			//mtx.unlock();
		} else if (cmd == "exit") {
			return 0;
		} else if (cmd == "add") {
			string dept="", course="", section="";
			cout << "Enter Department Number: ";
			getline(cin, dept);
			cout << "Enter Course Number: ";
			getline(cin, course);
			cout << "Enter Section Number: ";
			getline(cin, section);
			if (section.length() == 1)
				section.insert(0, "0");
			if (addCourseToList(dept, course, section))
				cout << dept<<':'<<course<<':'<<section<< " is now being spotted." << endl;
			else
				cerr << "ERROR: Unable to retrieve Rutgers data or " <<dept<<':'<<course<<':'<<section<< " is invalid." << endl;
		} else if (cmd.substr(0, 4) == "add ") {
			boost::cmatch what; // what[1]=dept, what[2]=course, what[3]=section
			boost::regex re("^add\\s+([0-9]{3})[\\s:]+([0-9]{3})[\\s:]+([0-9]{1,2})\\s*$");
			if (!boost::regex_match(cmd.c_str(), what, re)) {
				cout << "Error: Invalid syntax\n\tExample:\t198:111:01 or 198 111 01 where 198 is department, 111 is course, and 01 is section." << endl;
				continue;
			}
			string dept(what[1].first, what[1].second);
			string course(what[2].first, what[2].second);
			string section(what[3].first, what[3].second);
			if (section.length() == 1)
				section.insert(0, "0");
			if (addCourseToList(dept, course, section))
				cout << dept<<':'<<course<<':'<<section<< " is now being spotted." << endl;
			else
				cerr << "ERROR: Unable to retrieve Rutgers data or " <<dept<<':'<<course<<':'<<section<< " is invalid." << endl;
		} else if (cmd == "rm" || cmd == "remove" || cmd.substr(0,3) == "rm " || cmd.substr(0,7) == "remove ") {
			boost::regex reNumOnly("^\\s*([0-9]+)\\s*$");
			boost::regex reWithKeyword("^\\s*(?:rm|remove)\\s+([0-9]+)\\s*$");
			boost::cmatch what; // what[1]=dept, what[2]=course, what[3]=section
			if (cmd == "rm" || cmd == "remove") {
				printSpotList();
				cout << "Enter the row # containing the course to remove: ";
				getline(cin, cmd);
				if (!boost::regex_match(cmd.c_str(), what, reNumOnly)) {
					cout << "Error: Enter a number next time!" << endl;
					continue;
				}
			} else {
				if (!boost::regex_match(cmd.c_str(), what, reWithKeyword)) {
					cout << "Error: Invalid syntax for removal. Type help for the correct syntax." << endl;
					continue;
				}
			}
			string row(what[1].first, what[1].second);
			if (removeCourseFromList(atoi(row.c_str())))
				cout << "Removed course successfully!" << endl;
			else
				cerr << "ERROR: Course not removed due to invalid row specified." << endl;
		} else if (cmd == "info") {
			printProgramInfo();
		} else if (cmd == "silent") {
			info.silent = !info.silent;
			if (info.silent)
				cout << "Silent mode enabled - suppressing messages while spotting." << endl;
			else
				cout << "Silent mode disabled - displaying messages while spotting." << endl;
		} else if (cmd == "alert") {
			info.alert = !info.alert;
			if (info.alert)
				cout << "Alert enabled - an alerting sound will play and an SMS text message will be sent when a course is spotted." << endl;
			else
				cout << "Alert disabled - an alerting sound won't play and there will be no text message sent." << endl;
		} else if (cmd == "testSMS") {
			testSMS();
		} else {
			cout << "\nList of commands:\n";
			cout << "  alert               - enable/disable alert sound on course spot\n";
			cout << "  exit                - close the program\n";
			cout << "  info                - show spotter info\n";
			cout << "  list                - list all courses being spotted\n";
			cout << "  rm | remove         - remove a course being spotted\n";
			cout << "  rm <row>            - remove a course being spotted, where row is # from list\n";
			cout << "  silent              - enable/disable messages while spotting\n";
			cout << "  add <###:###:##>    - add a course to spot\n";
			cout << "  add <### ### ##>    - add a course to spot\n";
			cout << "  add                 - add a course to spot\n";
			cout << "  start               - start the spotter\n";
			cout << "  stop                - stop the spotter\n";
			cout << "  testSMS             - send a test SMS message to your phone\n";
			cout << "\nFor more details on the commands and the configuration file, check the README." << endl;
		}
	} while (1);

	return 0;
}

void __attribute__ ((destructor)) dtor() 
{
	curl_slist_free_all(curl.headers.json);
	curl_easy_cleanup(curl.handle);
	curl_global_cleanup();
}
