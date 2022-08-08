#include <iostream>
#include <string>

using namespace std;

// Решите загадку: Сколько чисел от 1 до 1000 содержат как минимум одну цифру 3?
int main() {
	int cnt = 0;
	for (int i = 1; i <= 1000; i++) {
		string number;
		number = to_string(i);
		for (auto j : number) {
			if (j == '3') {
				cnt++;
				break;
			}
		}
	}
	cout << cnt << endl;

}
