#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string>
#include <memory.h>

#define SSD_IPC "/tmp/ssd_apm_input"
#define SVC_IPC "/tmp/brown_svc_input"

typedef enum
{
  IPC_KEYEVENT = 0,
  IPC_COMMAND,
  IPC_LOGCMD,
  IPC_EVENT_MAX,
} IPC_EVENT_TYPE;

typedef enum
{
  IPC_COMMAND_EXIT = 0,
  IPC_COMMAND_SUSPEND,
  IPC_COMMAND_RESUME,
  IPC_COMMAND_RELOAD,
  IPC_COMMAND_BROWN_GETFOCUS,
  IPC_COMMAND_BROWN_LOSEFOCUS,
  IPC_COMMAND_APP_START_DONE,
  IPC_COMMAND_APP_STOP_DONE,
  IPC_COMMAND_SETUP_WATERMARK,
  IPC_COMMAND_APP_START,
  IPC_COMMAND_APP_STOP,
  IPC_COMMAND_MAX,
} IPC_COMMAND_TYPE;

typedef struct {
  uint32_t EventType;
  uint32_t Data;
  char StrData[256];
} IPCEvent;

class IPCOutput {
public:
  IPCOutput(const std::string& file):m_fd(-1), m_file(file) {
  }

  virtual ~IPCOutput() {
    Term();
  }

  bool Init() {
    if (m_fd < 0) {
      m_fd = open(m_file.c_str(), O_WRONLY | O_NONBLOCK);
    }
    return m_fd >= 0;
  }

  void Term() {
    if (m_fd >= 0) {
      close(m_fd);
      m_fd = -1;
    }
  }

  void Send(const IPCEvent& evt) {
    if (m_fd >= 0) {
      write(m_fd, &evt, sizeof(IPCEvent));
    }
  }

private:
  int m_fd;
  std::string m_file;
};


class IPCNameFifo {
public:
  IPCNameFifo(const char* file): m_file(file) {
    unlink(m_file.c_str());
    m_valid = !mkfifo(m_file.c_str(), 0777);
  }

  ~IPCNameFifo() {
    unlink(m_file.c_str());
  }

  inline const std::string& Path() { return m_file; }
  inline bool IsValid() { return m_valid; }

private:
  bool m_valid;
  std::string m_file;
};

class IPCInput {
public:
  IPCInput(const std::string& file):m_fd(-1),m_file(file),m_fifo(file.c_str()){}

  virtual ~IPCInput() {
    Term();
  }
  bool Init() {
    if (!m_fifo.IsValid()){
        printf("%s non-existent!!!! \n",m_fifo.Path().c_str());
        return false;
    }
    if (m_fd < 0) {
      m_fd = open(m_file.c_str(), O_RDWR | O_NONBLOCK);
    }
    return m_fd >= 0;
  }

  int Read(IPCEvent& evt) {
    if (m_fd >= 0) {
      return read(m_fd, &evt, sizeof(IPCEvent));
    }
    return 0;
  }

  void Term() {
    if (m_fd >= 0) {
      close(m_fd);
      m_fd = -1;
    }
  }

private:
  int m_fd;
  std::string m_file;
  IPCNameFifo m_fifo;
};
#if 0
int main(int argc, char *argv[]) {
  IPCEvent evt;
  memset(&evt,0,sizeof(IPCEvent));
  if(argc == 3){
    evt.EventType = atoi(argv[1]);
    evt.Data = atoi(argv[2]);
  }else if(argc == 2){
    evt.EventType = IPC_KEYEVENT;
    evt.Data = atoi(argv[1]);
  }else if(argc == 4){
    evt.EventType = atoi(argv[1]);
    evt.Data = atoi(argv[2]);
    strncpy(evt.StrData,argv[3],strlen(argv[3]));
  }else{
    printf("invalid parameter!\n");
    return 0;
  }
  printf("evt.EventType[%d] evt.Data[%d] evt.StrData[%s]\n",evt.EventType,evt.Data,evt.StrData);
  IPCOutput o(SVC_IPC);
  if (o.Init()) {
    o.Send(evt);
  }else{
    printf("No reading process!!!\n");
  }
  o.Term();
  return 0;
}
#else
int main(int argc, char *argv[]) {

  IPCEvent getevt;
  IPCEvent sendevt;
  bool brownexit;
  //step 1: created SSD pipe
  IPCInput ssdinput(SSD_IPC);
  if(!ssdinput.Init()){
    printf("SSD IPC Createfail");
    return 0;
  }

  //step 2: Check Browser bg process start down

  IPCOutput o(SVC_IPC);
  if(!o.Init()){
    printf("Brown process Not start!!!\n");
    o.Term();
    ssdinput.Term();
    return 0;
  }

  //step 3: send wake up evnt to Browser bg process
  printf("Wake up APP\n");
  memset(&sendevt,0,sizeof(IPCEvent));
  sendevt.EventType = IPC_COMMAND;
  sendevt.Data = IPC_COMMAND_APP_START;  //IPC_COMMAND_APP_STOP to stop browser fg
  //if homeurl is none ,use default url in run.sh
  //std::string homeurl = "http://www.baidu.com";
  //strncpy(sendevt.StrData,homeurl.c_str(),homeurl.size());
  o.Send(sendevt);
  brownexit = false;


  //step 4: wait IPC_COMMAND_APP_START_DONE and  IPC_COMMAND_APP_STOP_DONE to check Brown fg is active
  while(!brownexit){
    //memset 0 befor read
    memset(&getevt,0,sizeof(IPCEvent));
    if(ssdinput.Read(getevt) > 0){
      printf("Get EventEventType[%d] Data[%d] StrData[%s]\n",getevt.EventType,getevt.Data,getevt.StrData);
      if(getevt.EventType == IPC_COMMAND && getevt.Data == IPC_COMMAND_APP_START_DONE){
        printf("Browser start done!!!!\n");
      }

      if(getevt.EventType == IPC_COMMAND && getevt.Data == IPC_COMMAND_APP_STOP_DONE){
        printf("Browser Stop done!!!!\n");
        brownexit = true;
      }
      sleep(1);
    }
  }

  printf("Brownexit is %d exit demo process\n",brownexit);

  o.Term();
  ssdinput.Term();
  return 0;
}

#endif
