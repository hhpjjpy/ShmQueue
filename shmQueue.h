///////////////////////////////////////////////////////////
//  CZShmQueue.h
//  Implementation of the Class CZShmQueue
//  Created on:      24-6��-2019 8:53:20
//  Original author: Uncleyu
///////////////////////////////////////////////////////////

#if !defined(EA_1BEDCCA8_F1DB_47c5_815B_AFFCDA846E13__INCLUDED_)
#define EA_1BEDCCA8_F1DB_47c5_815B_AFFCDA846E13__INCLUDED_

/**
 * �ṩ�����ڴ����
 */

#include <iostream>
#include <string>
#include <sys/types.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>

#define SHMQUEUEOK 1
#define SHMQUEUEERROR -1

#define CheckAccess access
#define MAKE_DIR(path)          mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)

#include "CZGlobal.h"
#include "CZUtility.h"

using namespace std;

using namespace caasaii::global;
using namespace caasaii::utility;

namespace caasaii {
namespace shmqueue {

		static int createLocalPath(const char* path)
		{
			if (-1 == CheckAccess(path, 0))
			{
				if (-1 == MAKE_DIR(path))
					return -1;
			}
			return 0;
		}



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

			int Init(std::string sName) {

			    int fd_key;
				key_t key;
				char keyfile[256];

				char sys_tmp_path[128] = {0};
				char *pTmpPath = sys_tmp_path;

				CZUtility::readCfg("caasaii.config", "sys_tmp_path", &pTmpPath);
				createLocalPath(sys_tmp_path);

				sprintf(keyfile, "%s.%s", sys_tmp_path, sName.data());
				if (-1 == (fd_key = open(keyfile,O_CREAT,0644))) 
					return -1;

				close(fd_key);
				if (-1 == (key = ftok(keyfile, (int)'S')))
					return -1;
				
				m_iSemId = ::semget(key, 1, IPC_CREAT | IPC_EXCL | 0660);

				if (m_iSemId >= 0) {
					m_isCreate = 1;
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
				} 
				else {

					if (17 == errno)
					{
						m_iSemId = ::semget(key, 1, 0660);

						if (m_iSemId < 0) {
							m_sErrMsg.clear();
							m_sErrMsg = "semget error ";
							return m_iSemId;
						}
									
						m_isCreate = 1;
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

					} else {

					   return -2;
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

		class QueueHead {
		public:
			uint32_t uDataCount = 0;
			uint32_t uFront = 0;
			uint32_t uRear = 0;
			uint32_t uAllCount = 0;
			uint32_t uAllSize = 0;
			uint32_t uItemSize = 0;
		};
		
		static const uint32_t SHMSIZE = sizeof(DataCoprocessor) * sizeof(UnitCmd) + sizeof(QueueHead);//4*1024;//测试将大小调小，实际使用时应该设大避免影响性能

		template<typename T>
		class CZShmQueue {
		public:
		explicit CZShmQueue(const char *sName, int iCreate=-1):m_pShm(NULL),m_iStatus(-1), m_iShmId(-1), m_iCreate(iCreate){
				m_pShm = NULL;
				m_sName = sName;
			}

			~CZShmQueue() {
				::shmdt(m_pShm);
			}
			int Init(){

				int ret = m_Sem.Init(m_sName);
				if (ret < 0) {
					m_sErrMsg.clear();
					m_sErrMsg = m_Sem.GetErrMsg();
					return ret;
				}


				int fd_key;
				key_t key;
				char keyfile[256];

				char sys_tmp_path[128] = {0};
				char *pTmpPath = sys_tmp_path;

				CZUtility::readCfg("caasaii.config", "sys_tmp_path", &pTmpPath);
				createLocalPath(sys_tmp_path);

				sprintf(keyfile, "%s.%s", sys_tmp_path, m_sName.data());
				if (-1 == (fd_key = open(keyfile, O_CREAT, 0644)))
					return -1;
 
				close(fd_key);

				if (-1 == (key = ftok(keyfile, (int)'M')))
					return -1;

				m_iShmId = ::shmget(key,SHMSIZE,IPC_CREAT | 0660);

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
			
				QueueHead oQueryHead;
				oQueryHead.uItemSize = sizeof(T);
				oQueryHead.uAllSize = SHMSIZE - sizeof(QueueHead);
				oQueryHead.uAllCount = oQueryHead.uAllSize / oQueryHead.uItemSize;
				oQueryHead.uDataCount = 0;
				oQueryHead.uFront = 0;
				oQueryHead.uRear = 0;
				::memcpy(m_pShm,&oQueryHead,sizeof(QueueHead));
				
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

				cout << "----CZShmQueue:push" << ", share name = " << m_sName << __FILE__ << __FUNCTION__ << __LINE__ << endl;

				semLockGuard oLock(m_Sem);

				if (isFull())
					return -1;
				
				QueueHead oQueryHead;
				memcpy(&oQueryHead, m_pShm, sizeof(QueueHead));

				char *p = m_pShm + sizeof(QueueHead) + oQueryHead.uRear * sizeof(T);
				*((uint32_t*)(m_pShm + sizeof(int) * 2)) = (oQueryHead.uRear + 1) % oQueryHead.uAllCount;
				memcpy(p,&a,sizeof(T));

				return 0;
			}

			T pop() {

				cout << "----CZShmQueue:pop" << ", share name = " << m_sName <<  __FILE__ << __FUNCTION__ << __LINE__ << endl;

				semLockGuard oLock(m_Sem);

				if (isEmpty()) {
					return T();		//应该选择抛出异常等方式，待改进；
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
			char* m_pShm;
			int   m_iShmId;
			int   m_iStatus;
			int   m_iCreate;
			SemLock m_Sem;
			std::string m_sName;
			std::string m_sErrMsg;
		};

}
}

#endif // !defined(EA_1BEDCCA8_F1DB_47c5_815B_AFFCDA846E13__INCLUDED_)
