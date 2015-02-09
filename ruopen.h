#ifndef ruopen_h
#define ruopen_h

#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <curl/curl.h>
#include <string>
#include <jsoncpp/json/json.h>
#include <boost/regex.hpp>
#include <list>
#include <algorithm>
#include <boost/thread/thread.hpp>

#define CONFFILE "ruopen.conf"

using namespace std;

struct Curl {
	CURL *handle;
	CURLcode res;
	string response; /*response*/
	int respLen; /*response length*/
	string responseHeader; /*response header*/
	string cookiejar;
	struct Headers {
		struct curl_slist *json;
		struct curl_slist *text;
	} headers;
};
extern Curl curl;


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
	bool enableWebreg;
	bool enableSMS;
	bool silent;
	bool alert;
};

struct Department;
struct Course;
struct Section;
typedef list<Department> ListDepts;
typedef list<Course> ListCourses;
typedef list<Section> ListSections;

struct Section {
	string section;
	string courseIndex;
	int spotCounter; /*usually starting at 300 and decrements every spot*/

	/* Assignment operator*/
	bool operator ==(const Section &other) {
		return section == other.section && courseIndex == other.courseIndex;
	}
};

struct Course {
	string course;
	string courseCode;
	int json_index;
	ListSections sections;

	/* Assignment operator*/
	bool operator ==(const Course &other) {
		return course == other.course && courseCode == other.courseCode;
	}
};

struct Department {
	string dept;
	string deptCode;
	ListCourses courses;

	/* Assignment operator*/
	bool operator ==(const Department &other) {
		return dept == other.dept && deptCode == other.deptCode;
	}
};


bool addCourseToList(const string, const string, const string);
void createConfFile();
string createParams(const string);
void __attribute__ ((destructor)) dtor();
Json::Value *getCoursesJSON(const string);
string getCurrentSemester(); 
bool getDepartmentsJSON();
bool init();
inline void prinaProgramInfo();
void printSpotList();
void registerForCourse(const Section &);
bool removeCourseFromList(int);
bool setCampus(string);
bool setSemester(const string);
void sleep(int, int, bool);
void spawnAlert(const Department &, const Course &, const Section &);
inline void testSMS();
void thread_spot();
int writeCallback(char *, size_t, size_t, string *);


/*utils.cpp*/

/*Used for Email libcurl*/
struct upload_status {                                                    
	int lines_read;
};
const string currentDateTime();
extern list<string> providerEmails;
extern char payload_text[][70];
size_t payload_source(void *, size_t, size_t, void *);
string semesterCodeToString(const string);
string semesterStringToCode(string);


#endif
