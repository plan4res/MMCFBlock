##############################################################################
################################ makefile ####################################
##############################################################################
#                                                                            #
#   makefile of MMCFBlock                                                    #
#                                                                            #
#   Note that $(SMS++INC) is assumed to include any -I directive             #
#   corresponding to external libraries needed by SMS++, at least to the     #
#   extent in which they are needed by the parts of SMS++ used by MMCFBlock. #
#                                                                            #
#   Input:  $(CC)       = compiler command                                   #
#           $(SW)       = compiler options                                   #
#           $(SMS++INC) = the -I$( core SMS++ directory )                    #
#           $(SMS++OBJ) = the libSMS++ library itself                        #
#           $(MMCFSDR) = the directory where the source is                   #
#                                                                            #
#   Output: $(MMCFOBJ) = the final object(s) / library                       #
#           $(MMCFH)   = the .h files to include                             #
#           $(MMCFINC) = the -I$( source directory )                         #
#                                                                            #
#                              Antonio Frangioni                             #
#                         Dipartimento di Informatica                        #
#                             Universita' di Pisa                            #
#                                                                            #
##############################################################################


# macroes to be exported- - - - - - - - - - - - - - - - - - - - - - - - - - -

MMCFOBJ = $(MMCFSDR)MMCFBlock.o

MMCFINC = -I$(MMCFSDR)

MMCFH   = $(MMCFSDR)MMCFBlock.h

# clean - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

clean::
	rm -f $(MMCFOBJ) $(MILPBSDR)*~

# dependencies: every .o from its .C + every recursively included .h- - - - -

$(MMCFSDR)MMCFBlock.o: $(MMCFSDR)MMCFBlock.cpp \
	$(MMCFSDR)MMCFBlock.h $(SMS++OBJ)
	$(CC) -c $*.cpp -o $@ $(MMCFINC) $(SMS++INC) $(SW)

########################## End of makefile ###################################
