#include "ruopen.h"

struct Curl {
	CURL *handle;
	CURLcode res;
	string response; //response
	int respLen; //response length
	string responseHeader; // response header
	string cookiejar;
	struct Headers {
		struct curl_slist *json;
		struct curl_slist *text;
	} headers;
};

struct Info {
	string semester;
	string semesterString;
	string campus;
	string campusString;
	string smsNumber;
	string smsEmail;
	string smsPassword;
	string netid;
	string netidPassword;
	bool quiet;
};

struct Section {
	string section;
	string courseIndex;
	int spotCounter; //usually starting at 300 and decrements every spot

	// Assignment operator.
	bool operator ==(const Section &other) {
		return section == other.section && courseIndex == other.courseIndex;
	}
};

struct Course {
	string course;
	string courseCode;
	int json_index;
	ListSections sections;

	// Assignment operator.
	bool operator ==(const Course &other) {
		return course == other.course && courseCode == other.courseCode;
	}
};

struct Department {
	string dept;
	string deptCode;
	ListCourses courses;

	// Assignment operator.
	bool operator ==(const Department &other) {
		return dept == other.dept && deptCode == other.deptCode;
	}
};

Curl curl;
static Info info;
static ListDepts spotting;
static Json::Value dept_json;

//Multithreading variables
static boost::mutex mtx;
static bool spotting_bool; //lets spot thread know when to terminate

#include "utils.cpp"

inline void printInfo() {
	cout << "\nRUopen Version 1.0   -   Created by codeniko\n----------------------------------------" << endl;
	cout << "Semester: " << info.semesterString << endl << "Campus: " << info.campusString << endl;
	cout << "SMS Email : " << info.smsEmail << endl << "SMS Phone Number: " << info.smsNumber << endl;
}

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

	curl_easy_setopt(curl.handle, CURLOPT_USERAGENT, "Mozilla/5.0 (X11; Linux x86_64; rv:17.0) Gecko/20130917 Firefox/17.0 Iceweasel/17.0.9");
	curl_easy_setopt(curl.handle, CURLOPT_NOPROGRESS, 1); // turn of progress bar
	// curl_easy_setopt(curl.handle, CURLOPT_FAILONERROR, 1); // fail on error
	curl_easy_setopt(curl.handle, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl.handle, CURLOPT_WRITEFUNCTION, writeCallback);
	curl_easy_setopt(curl.handle, CURLOPT_WRITEDATA, &curl.response);
	curl_easy_setopt(curl.handle, CURLOPT_AUTOREFERER, 1);
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
	info.quiet = false;
	setCampus("new brunswick");
	return setSemester(getCurrentSemester());
}

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

string getCurrentSemester()
{
	curl_easy_setopt(curl.handle, CURLOPT_HTTPHEADER, curl.headers.json);
	curl_easy_setopt(curl.handle, CURLOPT_URL, "http://sis.rutgers.edu/soc/current_term_date.json");
	curl_easy_setopt(curl.handle, CURLOPT_REFERER, "http://sis.rutgers.edu/soc");
	curl.res = curl_easy_perform(curl.handle);

	if (curl.res != CURLE_OK)
		return "NULL";
	else {
		Json::Value jsonroot;
		Json::Reader jsonreader;
		if (!jsonreader.parse(curl.response, jsonroot)) {
			cerr << "ERROR: Json parser errored on parsing semester information." << endl;
			return "NULL";
		}
		int year = jsonroot.get("year", -1).asInt();
		int term = jsonroot.get("term", -1).asInt();
		if (year == -1 || term == -1) {
			cerr << "ERROR: Wrong semester information retrieved." << endl;
			return "NULL";
		}

		//If Summer or Winter, change to previous Spring or Fall
		if (term == 0) { //Winter -> Fall
			term = 9;
			--year;
		}
		if (term == 7) //Summer -> Spring
			term = 1;

		stringstream ss;
		ss << term << year; //rutgers semester code is just term + year
		curl.response.clear();
		return semesterCodeToString(ss.str());
	}
}

string createParams(string dept = "NULL")
{
	stringstream ss;
	ss << "semester=" << info.semester << "&campus=" << info.campus << "&level=U,G";
	if (dept != "NULL")
		ss << "&subject=" << dept;
	return ss.str();
}

bool getDepartments()
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
		//cerr << "ERROR: Unable to retrieve Rutgers data." << endl;
		return false;
	}

	Json::Reader jsonreader;
	if (!jsonreader.parse(curl.response, dept_json) || dept_json.size() == 0) {
		//cerr << "ERROR: Unable to retrieve Rutgers data." << endl;
		return false;
	}

	curl.response.clear();
	alreadyRetrieved = true;
	return true;
}

Json::Value *getCourses(string &deptcode)
{
	string params = createParams(deptcode);
	curl_easy_setopt(curl.handle, CURLOPT_HTTPHEADER, curl.headers.json);
	curl_easy_setopt(curl.handle, CURLOPT_URL, string("http://sis.rutgers.edu/soc/courses.json?").append(params).c_str());
	curl_easy_setopt(curl.handle, CURLOPT_REFERER, "http://sis.rutgers.edu/soc");
	curl.res = curl_easy_perform(curl.handle);
	if (curl.res != CURLE_OK) {
		cerr << "ERROR: Unable to retrieve Rutgers data." << endl;
		return NULL;
	}

	Json::Value *courses = new Json::Value();
	Json::Reader jsonreader;
	if (!jsonreader.parse(curl.response, *courses) || courses->size() == 0) {
		cerr << "ERROR: Unable to retrieve Rutgers data." << endl;
		return NULL;
	}

	curl.response.clear();
	return courses;
}

bool setSemester(string semesterString)
{
	string code;
	if (semesterString == "NULL" || (code = semesterStringToCode(semesterString)) == "NULL")
		return false;

	info.semester = code;
	info.semesterString = semesterCodeToString(code);
	return true;
}

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

bool removeCourse(int row)
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

//Add a course to spot
bool spotCourse(string &deptcode, string &coursecode, string &sectioncode)
{
	if (!getDepartments())
		return false;

	//Validate Department Input
	Json::ValueIterator it, it2, it3;
	for (it = dept_json.begin(); it != dept_json.end(); ++it) {
		if ((*it).get("code", "NULL").asString() == deptcode)
			break;
	}
	if (it == dept_json.end())
		return false;
	Department dept;
	dept.dept = (*it).get("description", "NULL").asString();
	dept.deptCode = deptcode;

	//Validate Course Input
	Json::Value *course_json = getCourses(dept.deptCode);
	if (course_json == NULL)
		return false;
	unsigned int c;
	for (c = 0; c < (*course_json).size(); ++c) {
		if ((*course_json)[c].get("courseNumber", "NULL").asString() == coursecode)
			break;
	}
	if (c == (*course_json).size()) {
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

void createConfFile()
{
	ofstream conf;
	conf.open(CONFFILE);
	conf << "# NOTE* If not using a setting or want the default, you can also remove the setting entirely or have the value be an empty line.\n" <<
		"# [CAMPUS] Values: New Brunswick, Newark, Camden.\n# Default: New Brunswick\n\n" <<
		"# [SEMESTER] Values: <SEASON> <YEAR>\n# Examples: Spring 2014, Summer 2013, Fall 2012, Winter 2011, etc...\n# Default: <EMPTY LINE> signifying to use current semester (determined automatically).\n\n" <<
		"# [COURSES] Values: <DEPARTMENT>:<COURSE>:<SECTION>\n# Examples: 198:111:03 or 640:250:01\n# Default: <EMPTY LINE> signifying not to spot any courses (can be set within program)." <<
		"#     NOTE* Have each course on a separate line with no empty lines inbetween.\n\n"
		"[CAMPUS]\nNew Brunswick\n\n[SEMESTER]\n\n" <<
		"[NETID]\n\n[NETID PASSWORD]\n\n" <<
		"[SMS EMAIL]\nexample@yahoo.com\n\n[SMS PASSWORD]\nPasswordForEmailGoesHere\n\n" <<
		"[SMS PHONE NUMBER]\n1234567890\n\n[QUIET]\nfalse\n\n[COURSES]\n\n";

	conf.close();
}

void printSpotting()
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

void registerForCourse(Section &section)
{
	curl_easy_setopt(curl.handle, CURLOPT_HTTPHEADER, curl.headers.text);
	curl_easy_setopt(curl.handle, CURLOPT_REFERER, "https://sims.rutgers.edu/webreg/chooseSemester.htm?login=cas"); //default
	curl_easy_setopt(curl.handle, CURLOPT_URL, "https://cas.rutgers.edu/login?service=https%3A%2F%2Fsims.rutgers.edu%2Fwebreg%2Fj_acegi_cas_security_check");
	curl.res = curl_easy_perform(curl.handle);
	if (curl.res != CURLE_OK) {
		cerr << "ERROR: Unable to login into webreg 1." << endl;
		curl.response.clear();
		return;
	}

	boost::cmatch what; // what[1]=dept, what[2]=course, what[3]=section
	boost::regex re("input type=\"hidden\" name=\"lt\" value=\"_cNoOpConversation (.+?)\"");
	if (!boost::regex_search(curl.response.c_str(), what, re)) {
		cerr << "ERROR: Unable to login into webreg 2." << endl;
		curl.response.clear();
		return;
	}
	string lt(what[1].first, what[1].second);
	curl.response.clear();

	string postfields = "username="+info.netid+"&password="+info.netidPassword+"&authenticationType=Kerberos&lt=_cNoOpConversation " + lt + "&_currentStateId=&_eventId=submit";
	curl_easy_setopt(curl.handle, CURLOPT_URL, "https://cas.rutgers.edu/login?service=https%3A%2F%2Fsims.rutgers.edu%2Fwebreg%2Fj_acegi_cas_security_check");
	curl_easy_setopt(curl.handle, CURLOPT_POSTFIELDS, postfields.c_str());
	curl.res = curl_easy_perform(curl.handle);
	if (curl.res != CURLE_OK) {
		cerr << "ERROR: Unable to login into webreg 3." << endl;
		curl.response.clear();
		return;
	}
	curl.response.clear();

	postfields = "semesterSelection="+info.semester+"&submit=Continue+&#8594";
	curl_easy_setopt(curl.handle, CURLOPT_REFERER, "https://sims.rutgers.edu/webreg/chooseSemester.htm"); //default
	curl_easy_setopt(curl.handle, CURLOPT_URL, "https://sims.rutgers.edu/webreg/editSchedule.htm");
	curl_easy_setopt(curl.handle, CURLOPT_POSTFIELDS, postfields.c_str());
	curl.res = curl_easy_perform(curl.handle);
	if (curl.res != CURLE_OK) {
		cerr << "ERROR: Unable to login into webreg 4." << endl;
		curl.response.clear();
		return;
	}
	curl.response.clear();

	postfields = "coursesToAdd[0].courseIndex="+section.courseIndex+"&coursesToAdd[1].courseIndex=&coursesToAdd[2].courseIndex=&coursesToAdd[3].courseIndex=&coursesToAdd[4].courseIndex=&coursesToAdd[5].courseIndex=&coursesToAdd[6].courseIndex=&coursesToAdd[7].courseIndex=&coursesToAdd[8].courseIndex=&coursesToAdd[9].courseIndex=";
	curl_easy_setopt(curl.handle, CURLOPT_URL, "https://sims.rutgers.edu/webreg/addCourses.htm");
	curl_easy_setopt(curl.handle, CURLOPT_POSTFIELDS, postfields.c_str());
	curl.res = curl_easy_perform(curl.handle);
	if (curl.res != CURLE_OK) {
		cerr << "ERROR: Unable to login into webreg 5." << endl;
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

inline void testSMS() {
	Department dept;
	Course course;
	Section section;
	dept.dept = "TESTSMS";
	cout << "Sending a test message to " << info.smsNumber << " from " << info.smsEmail << "." << endl;
	spotted(dept, course, section);
}

//A course has been spotted! Alert the user by playing a sound file and sending an SMS
void spotted(Department &dept, Course &course, Section &section)
{
	string whatspotted = "This is a test message from RUopen.";
	if (dept.dept != "TESTSMS") {
		section.spotCounter = 200;
		whatspotted = "["+section.courseIndex+"] "+dept.deptCode+":"+course.courseCode+":"+section.section+" "+course.course+" has been spotted!\r\n";
		cout << whatspotted << flush;
		system("mpg321 -q alert.mp3");
	}
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

	registerForCourse(section);
}

//main spotting thread
void spot()
{
	cout << "Spotter started!" << endl;
	if (info.quiet)
		cout << "Quiet mode is enabled - suppressing messages while spotting." << endl;
	while (1) {
		//should thread die?
		mtx.lock();
		if (!spotting_bool){
			mtx.unlock();
			return;
		}
		mtx.unlock();

		int sleeptime = rand() % 3000 + 1001;
		for (ListDepts::iterator dept = spotting.begin(); dept != spotting.end(); ++dept) {
			Json::Value *course_json = getCourses(dept->deptCode);
			if (course_json == NULL) {
				if (!info.quiet)
					cout << "Unable to retrieve courses, trying again in " << sleeptime+5000 << "ms." << endl;
				boost::this_thread::sleep(boost::posix_time::milliseconds(sleeptime+5000));
			}
			for (ListCourses::iterator course = dept->courses.begin(); course != dept->courses.end(); ++course) {
				Json::Value section_json = (*course_json)[course->json_index]["sections"];
				for (ListSections::iterator section = course->sections.begin(); section != course->sections.end(); ++section) {
					for (unsigned int s = 0; s < section_json.size(); ++s) {
						if (section_json[s].get("number", "NULL").asString() == section->section) {
							if (!info.quiet)
								cout << "Checking "<<'[' << section->courseIndex << "] " << dept->deptCode << ":" << course->courseCode << ":" << section->section << ' ' << course->course << ".......";
							if (section_json[s].get("openStatus", false).asBool() == true && section->spotCounter <= 0) {
								cout << "OPEN!" << endl;
								spotted(*dept, *course, *section);
							} else
								if (!info.quiet)
									cout << "closed." << endl;;
						}
					}
					--section->spotCounter;
				}
			}
			delete course_json;
		}
		if (!info.quiet)
			cout << "Sleeping for " << sleeptime << "ms." << endl;
		boost::this_thread::sleep(boost::posix_time::milliseconds(sleeptime));
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
			printInfo();
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
				if (!spotCourse(dept, course, section))
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
		} else if (line == "[QUIET]") {
			getline(conf, line);
			if (line == "true")
				info.quiet = true;
			else
				info.quiet = false;
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

	printSpotting();

	do {
		cout << endl << "Enter a command: ";
		string cmd;
		getline(cin, cmd);
		if (cmd == "list")
			printSpotting();
		else if (cmd == "start") {
			if (spotting_bool) {
				cout << "Program is already spotting for courses!" << endl;
			} else {
				spotting_bool = true;
				boost::thread thread1(spot);
			}
		} else if (cmd == "stop") {
			cout << "Stopping the spotter....";
			mtx.lock();
			spotting_bool = false;
			mtx.unlock();
		} else if (cmd == "exit") {
			return 0;
		} else if (cmd == "spot") {
			string dept, course, section;
			cout << "Enter Department Number: ";
			getline(cin, dept);
			cout << "Enter Course Number: ";
			getline(cin, course);
			cout << "Enter Section Number: ";
			getline(cin, section);
			if (section.length() == 1)
				section.insert(0, "0");
			if (spotCourse(dept, course, section))
				cout << dept<<':'<<course<<':'<<section<< " is now being spotted." << endl;
			else
				cerr << "ERROR: Unable to retrieve Rutgers data or " <<dept<<':'<<course<<':'<<section<< " is invalid." << endl;
		} else if (cmd.substr(0, 5) == "spot ") {
			boost::cmatch what; // what[1]=dept, what[2]=course, what[3]=section
			boost::regex re("^spot\\s+([0-9]{3})[\\s:]+([0-9]{3})[\\s:]+([0-9]{1,2})\\s*$");
			if (!boost::regex_match(cmd.c_str(), what, re)) {
				cout << "Error: Invalid syntax\n\tExample:\t198:111:01 or 198 111 01 where 198 is department, 111 is course, and 01 is section." << endl;
				continue;
			}
			string dept(what[1].first, what[1].second);
			string course(what[2].first, what[2].second);
			string section(what[3].first, what[3].second);
			if (section.length() == 1)
				section.insert(0, "0");
			if (spotCourse(dept, course, section))
				cout << dept<<':'<<course<<':'<<section<< " is now being spotted." << endl;
			else
				cerr << "ERROR: Unable to retrieve Rutgers data or " <<dept<<':'<<course<<':'<<section<< " is invalid." << endl;
		} else if (cmd == "rm" || cmd == "remove") {
			printSpotting();
			cout << "Enter the row # containing the course to remove: ";
			getline(cin, cmd);
			boost::cmatch what; // what[1]=dept, what[2]=course, what[3]=section
			boost::regex re("^\\s*([0-9]+)\\s*$");
			if (!boost::regex_match(cmd.c_str(), what, re)) {
				cout << "Error: Enter a number next time!" << endl;
				continue;
			}
			string row(what[1].first, what[1].second);
			if (removeCourse(atoi(row.c_str())))
				cout << "Removed course successfully!" << endl;
			else
				cerr << "ERROR: Course not removed due to invalid row specified." << endl;
		} else if (cmd == "info") {
			printInfo();
		} else if (cmd == "quiet") {
			info.quiet = !info.quiet;
			if (info.quiet)
				cout << "Quiet mode enabled - suppressing messages while spotting." << endl;
			else
				cout << "Quiet mode disabled - displaying messages while spotting." << endl;
		} else if (cmd == "testSMS") {
			testSMS();
		} else {
			cout << "\nList of commands:\n";
			cout << "  exit                - close the program\n";
			cout << "  info                - show spotter info\n";
			cout << "  list                - list all courses being spotted\n";
			cout << "  quiet               - enable/disable messages while spotting\n";
			cout << "  rm | remove         - remove a course being spotted\n";
			cout << "  spot <###:###:##>   - add a course to spot\n";
			cout << "  spot <### ### ##>   - add a course to spot\n";
			cout << "  spot                - add a course to spot\n";
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
