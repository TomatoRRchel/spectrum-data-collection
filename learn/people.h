#ifndef _PEOPLE_H
#define _PEOPLE_H

#include<iostream>
#include<string>
using namespace std;


class people
{
	friend void vist(people& fri);
	friend class man;
	//friend man::operator++;
private:
	int age;
	//static char symbol;
	std::string name;
public:
	people(void);
	people(int _age);


	void set_age(int _age);
	int get_age() const;
	void set_name(string _name);
	string get_name();
	bool equal(people& temp);
	people& PersonAdd(people p);
	people add(people p);
};
class man {
	friend ostream& operator<<(ostream& temp, man& _man);
public:
	man(void);
	man(const man& _man);
	~man(void);
	man& operator++();
	man operator++(int);
	man& operator=(man& _man);
	void touch_age();
private:
	people* p;
};
#endif

