#include<iostream>
#include "people.h"
using namespace std;

people p;
people p1;

class cal {
public:
	cal() {
		num1 = 10;
		num2 = 20;
	}
	int num1, num2;
	virtual string fun() {
		return "None";
	}
	virtual int getresult() {
		return 0;
	}
};

class add :public cal {
public:
	virtual string fun() {
		return "add";
	}
	int getresult() {
		return num1 + num2;
	}
};

class cut :public cal {
public:
	virtual string fun() {
		return "cut";
	}
	int getresult() {
		return (num1 - num2) > 0 ? (num1 - num2) : -(num1 - num2);
	}
};

class mul :public cal {
public:
	virtual string fun() {
		return "mul";
	}
	int getresult() {
		return num1 * num2;
	}
};

void result(cal& _cal) {
	cout << _cal.num1 << _cal.fun() << _cal.num2 << "=" << _cal.getresult() << endl;
}
int main() {


	add a;
	cut b;
	mul c;
	result(a);
	result(b);
	result(c);

	//p.set_age(2);
	//p1.set_age(1);
	////p.PersonAdd(p1).PersonAdd(p1);
	//p.add(p1);
	//cout << "age:" << p.get_age() << endl;
	//vist(p);
	//const people a(10);
	//
	//cout << "age:" << a.get_age()<< endl;
	//man a, b;
	//a.touch_age();
	//cout << "my people age : " << a << endl;
	//++(++(++a));
	//cout << "now increase: " << a << endl;
	//a = b;
	//cout << "now ouput two age a::" << a << "----b::" << b << endl;
	//++b;
	//cout << "now ouput two age a::" << a << "----b::" << b << endl;
	system("pause");
	return 0;
}