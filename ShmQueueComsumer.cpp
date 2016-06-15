#include <iostream>
#include "shmQueue.h"

using namespace hhp::shmQueue;
int main() {
	shmQueue<int> oShmQueue;
	if (oShmQueue.Init() < 0) {
		std::cout << " init error  msg: " << oShmQueue.GetErrMsg() << std::endl;
	}
	int sum = 0;
	while (1) {
		while (!oShmQueue.isEmpty()) {
			int i = oShmQueue.pop();
			if (i == 0) {
				std::cout << "empty" << std::endl;
				break;
			}
			std::cout << i << std::endl;
			sum += i;
			std::cout << sum << std::endl;
		}
	}

}