#ifndef _ruopen_h_
#define _ruopen_h_

#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <curl/curl.h>
#include <string>
#include <json/json.h>
#include <boost/regex.hpp>
#include <list>
#include <algorithm>
//#include <boost/thread/locks.hpp> 
//#include <boost/thread/lock_guard.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>

#define CONFFILE "ruopen.conf"

using namespace std;

struct Department;
struct Course;
struct Section;
typedef list<Department> ListDepts;
typedef list<Course> ListCourses;
typedef list<Section> ListSections;


void createConfFile();
string createParams(string);
void __attribute__ ((destructor)) dtor();
Json::Value *getCourses(string &);
string getCurrentSemester();
bool getDepartments();
bool init();
inline void printInfo();
void printSpotting();
void registerForCourse(Department &, Course &, Section &);
bool removeCourse(int);
bool setCampus(string);
bool setSemester(string);
void spot();
bool spotCourse(string &, string &, string &);
void spotted(Department *, Course *, Section *);
inline void testSMS();
int writeCallback(char *, size_t, size_t, string *);


//utils.cpp
extern list<string> providerEmails;
extern char payload_text[][70];
struct upload_status;
size_t payload_source(void *, size_t, size_t, void *);
string semesterCodeToString(string);
string semesterStringToCode(string);
void debug();


#endif
