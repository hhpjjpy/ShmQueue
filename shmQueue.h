#pragma once
#ifndef SHMQUEUE_H
#define SHMQUEUE_H
#include <iostream>
#include <string>
#include <sys/sem.h>
#include <sys/shm.h>
#include <string.h>
#include <stdint.h>
#define SHMQUEUEOK 1
#define SHMQUEUEERROR -1
#define SHMKEY   20160615
#define SEMKEY   20160616

namespace hhp {
	namespace shmQueue {


		class SemLock {
		public:
			union semun {
				int val; /* value for SETVAL */

				struct semid_ds *buf; /* buffer for IPC_STAT, IPC_SET */

				unsigned short *array; /* array for GETALL, SETALL */

				struct seminfo *__buf; /* buffer for IPC_INFO */
			};

			SemLock() :m_iSemId(-1), m_isCreate(-1) {

			}
			~SemLock() {

			}
			int Init() {
				m_iSemId = ::semget(SEMKEY, 1, 0);
				if (m_iSemId < 0) {
					m_iSemId = ::semget(SEMKEY, 1, IPC_CREAT);
					m_isCreate = 1;
				}
				if (m_iSemId < 0) {
					m_sErrMsg.clear();
					m_sErrMsg = "semget error ";
					return m_iSemId;
				}
				if (m_isCreate == 1) {
					union semun arg;
					arg.val = 1;
					int ret = ::semctl(m_iSemId, 0, SETVAL, arg.val);
					if (ret < 0) {
						m_sErrMsg.clear();
						m_sErrMsg = "sem setval error ";
						return ret;
					}
				}
				return m_iSemId;
			}

			int Lock() {
				union semun arg;
				int val = ::semctl(m_iSemId, 0, GETVAL, arg);
				if (val == 1) {
					struct sembuf sops = { 0,-1, SEM_UNDO };
					int ret = ::semop(m_iSemId, &sops, 1);
					if (ret < 0) {
						m_sErrMsg.clear();
						m_sErrMsg = "semop -- error ";
						return ret;
					}
				}
				return 0;
			}
			int unLock() {
				union semun arg;
				int val = ::semctl(m_iSemId, 0, GETVAL, arg);
				if (val == 0) {
					struct sembuf sops = { 0,+1, SEM_UNDO };
					int ret = ::semop(m_iSemId, &sops, 1);
					if (ret < 0) {
						m_sErrMsg.clear();
						m_sErrMsg = "semop ++ error ";
						return ret;
					}
				}
				return 0;
			}
			std::string GetErrMsg() {
				return m_sErrMsg;
			}

		private:
			int m_iSemId;
			int m_isCreate;
			std::string m_sErrMsg;
		};

		class semLockGuard {
		public:
			semLockGuard(SemLock sem) :m_Sem(sem) {
				m_Sem.Lock();
			}
			~semLockGuard() {
				m_Sem.unLock();
			}
		private:
			SemLock &m_Sem;
		};


		static const uint32_t SHMSIZE = 4*100+24;//测试将大小调小，实际使用时应该设大避免影响性能

		class QueueHead {
		public:
			uint32_t uDataCount;
			uint32_t uFront;
			uint32_t uRear;
			uint32_t uAllCount;
			uint32_t uAllSize;
			uint32_t uItemSize;
		public:
			QueueHead() :uDataCount(0), uFront(0), uRear(0),
				uAllCount(0), uAllSize(0), uItemSize(0){

			}
		};


		template<typename T>
		class shmQueue {
		public:
		explicit shmQueue(int iCreate=-1):m_pShm(NULL),m_iStatus(-1), m_iShmId(-1), m_iCreate(iCreate){
				m_pShm = NULL;
			}

			~shmQueue() {
				::shmdt(m_pShm);
			}
			int Init(){
				int ret = m_Sem.Init();
				if (ret < 0) {
					m_sErrMsg.clear();
					m_sErrMsg = m_Sem.GetErrMsg();
					return ret;
				}
				m_iShmId = ::shmget(SHMKEY,SHMSIZE,0);
				if (m_iShmId < 0) {
					m_iCreate = 1;
					m_iShmId = ::shmget(SHMKEY, SHMSIZE, IPC_CREAT);
				}
				if (m_iShmId < 0) {
					m_sErrMsg.clear();
					m_sErrMsg = "shmget error ";
					m_iStatus = SHMQUEUEERROR;
					return m_iShmId;
				}

				m_pShm = (char*)::shmat(m_iShmId,NULL,0);//读写模式；
				if (m_pShm == NULL) {
					m_sErrMsg.clear();
					m_sErrMsg = "shmat error ";
					return -1;
				}
			
				if (m_iCreate == 1) {
					QueueHead oQueryHead;
					oQueryHead.uItemSize = sizeof(T);
					oQueryHead.uAllSize = SHMSIZE - 20;
					oQueryHead.uAllCount = oQueryHead.uAllSize / oQueryHead.uItemSize;
					oQueryHead.uDataCount = 0;
					oQueryHead.uFront = 0;
					oQueryHead.uRear = 0;
					::memcpy(m_pShm,&oQueryHead,sizeof(QueueHead));
				}
				
				return m_iShmId;
			}

			bool isEmpty() {
				QueueHead oQueryHead;
				memcpy(&oQueryHead,m_pShm,sizeof(QueueHead));

				return oQueryHead.uFront == oQueryHead.uRear;
			}

			bool isFull() {
				QueueHead oQueryHead;
				memcpy(&oQueryHead, m_pShm, sizeof(QueueHead));

				return oQueryHead.uFront == (oQueryHead.uRear + 1) % oQueryHead.uAllCount;
			}

			int getSize() {
				QueueHead oQueryHead;
				memcpy(&oQueryHead, m_pShm, sizeof(QueueHead));

				return oQueryHead.uDataCount;
			}

			int  push(T a){

				semLockGuard oLock(m_Sem);

				if (isFull()) {
					return -1;
				}
				QueueHead oQueryHead;
				memcpy(&oQueryHead, m_pShm, sizeof(QueueHead));

				char *p = m_pShm + sizeof(QueueHead) + oQueryHead.uRear * sizeof(T);
				*((uint32_t*)(m_pShm + sizeof(int) * 2)) = (oQueryHead.uRear + 1) % oQueryHead.uAllCount;
				memcpy(p,&a,sizeof(T));
				return 0;
			}

			T pop() {

				semLockGuard oLock(m_Sem);

				if (isEmpty()) {
					return T();//应该选择抛出异常等方式，待改进；
				}
				QueueHead oQueryHead;
				memcpy(&oQueryHead, m_pShm, sizeof(QueueHead));

				char *p = m_pShm + sizeof(QueueHead) + oQueryHead.uFront * sizeof(T);
				
				T odata;
				memcpy(&odata,p,sizeof(T));

				*((uint32_t*)(m_pShm+sizeof(int)*1))= ((oQueryHead.uFront+ 1) % oQueryHead.uAllCount);

				return odata;
			}

			std::string GetErrMsg() {
				return m_sErrMsg;
			}
		private:
			char *m_pShm;
			int m_iShmId;
			int m_iStatus;
			int m_iCreate;
			SemLock m_Sem;
			std::string m_sErrMsg;
		};


	}
}

#endif // !SHMQUEUE_H

