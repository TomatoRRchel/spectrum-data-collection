#include "people.h"

ostream& operator<<(ostream& temp, man& _man) {
	temp << _man.p->get_age();

	return temp;

}

man::man(void) {
	p = new people(23);
}
man::man(const man& _man) {
	if (p == NULL)
		p = new people(_man.p->age);
}
man::~man(void) {
	if (p != NULL)
		delete p;
}
man& man::operator++() {
	p->age++;
	return *this;
}

man man::operator++(int) {
	man temp = *this;
	p->age++;
	return temp;
}

man& man::operator=(man& _man) {
	//if (this == NULL) {
	//	*this = new man;
	//}
	p->age = _man.p->age;
	return *this;
}



void man::touch_age() {
	cout << "my people age" << this->p->age << endl;
}



people::people(void) {
	cout << "do nothing" << endl;
}

people::people(int _age) {
	age = _age;

}


void vist(people& fri) {
	cout << "private age:" << fri.age << endl;
}
void people::set_age(int _age) {
	people::age = _age;
}
int people::get_age() const {
	return this->age;
}
void people::set_name(std::string _name) {
	people::name = _name;
}
std::string people::get_name() {
	return people::name;
}

bool people::equal(people& temp) {
	return get_age() == temp.get_age();
}

people& people::PersonAdd(people p)
{
	this->age += p.age;
	//返回对象本身
	return *this;
}

people people::add(people p)
{
	this->age += p.age;
	//返回对象本身
	return *this;
}