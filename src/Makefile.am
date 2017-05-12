
INCLUDES= -I ../include -I /usr/include/libxml2

COMMON_APPLICATION_SOURCES= \
	CanSet.cpp       ConsoleExecuter.cpp     FullScreen.cpp  RemoteDirectoryUpdater.cpp  ScreenColors.cpp  TrashScreen.cpp \
	CommandLine.cpp  EventSerialization.cpp  JobQueue.cpp    RemoteTrashEvent.cpp        TrashCan.cpp      UserConfiguration.cpp

trailerprgdir=${exec_prefix}/bin
trailerprg_PROGRAMS=trailer
trailer_SOURCES=../exe/trailer.cpp $(COMMON_APPLICATION_SOURCES)

ttprgdir=${exec_prefix}/bin
ttprg_PROGRAMS=tt
tt_SOURCES=../exe/tt.cpp $(COMMON_APPLICATION_SOURCES)


