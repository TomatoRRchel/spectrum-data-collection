//#include<iostream>
//#include "people.h"
//people man;
//people woman;
//
////2. 地址传递
//void mySwap02(int* a, int* b) {
//	int temp = *a;
//	*a = *b;
//	*b = temp;
//}
//
////3. 引用传递
//void mySwap03(int& a, int& b) {
//	int temp = a;
//	a = b;
//	b = temp;
//}
//
//int main() {
//	woman.set_age(18);
//	man.set_age(18);
//	man.set_name("hello");
//	cout << man.get_age() << "  " << man.get_name() << endl;
//	cout << "compare age" <<man.equal(woman)<< "end" << endl;
//	int a = 10;
//	int b = 20;
//
//
//	mySwap02(&a, &b);
//	cout << "a:" << a << " b:" << b << endl;
//
//	mySwap03(a, b);
//	cout << "a:" << a << " b:" << b << endl;
//
//	return 0;
//}