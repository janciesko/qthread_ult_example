
#Make sure QT_INCLUDE_PATH and QT_LIB_PATH are defined and point to your QThreads installation directory


MPICC=mpicc
default: sendrecv_ult
sendrecv_ult: sendrecv_ult.c
	${MPICC} $^ -I/$(QT_INCLUDE_PATH) -L/${QT_LIB_PATH} -lqthread -o $@ 

clean:
	rm -f sendrecv_ult
