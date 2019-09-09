#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include "shmQueue.h"

using namespace caasaii::shmQueue;

int main() {

	shmQueue<int> oShmQueue("producer", 1);
	if (oShmQueue.Init() < 0) {
		std::cout << " init error  msg: " << oShmQueue.GetErrMsg() << std::endl;
	}
	int sum = 0;
	int i = 10000;
	while (i != 0) {
		if (oShmQueue.push(i) != 0){
			std::cout << "sleep 1s" << std::endl;
			sleep(1);
			continue ;
		}
		std::cout << " I; " << i << std::endl;
		sum += i;
		i--;
		std::cout << " sum; " << sum << std::endl;
	}
	
}
