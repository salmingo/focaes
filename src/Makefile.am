bin_PROGRAMS=focaes
focaes_SOURCES=ioservice_keep.cpp msgque_base.cpp tcp_asio.cpp mountproto.cpp termscreen.cpp \
               GLog.cpp \
               FileTransferClient.cpp \
               CameraBase.cpp \
               apgSampleCmn.cpp CameraApogee.cpp \
               udp_asio.cpp CameraGY.cpp \
               CameraTucam.cpp \
               focaes.cpp

focaes_LDFLAGS=-L/usr/local/lib
AM_CPPFLAGS=-I/usr/local/include/libapogee-3.0
COMMON_LIBS=-lpthread -lcurl -lm -lrt -lcfitsio -lxpa
BOOST_LIBS=-lboost_system-mt-x64 -lboost_thread-mt-x64 -lboost_date_time-mt-x64 -lboost_chrono-mt-x64
APOGEE_LIBS=-lapogee
TUCAM_LIBS=-lTUCam

focaes_LDADD=${COMMON_LIBS} ${BOOST_LIBS} ${APOGEE_LIBS} ${TUCAM_LIBS}
