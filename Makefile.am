
EXTRA_DIST=include eclipse-template replacer exe test

SUBDIRS=src

ECLIPSE_TRAILER_BASE=$(PWD)/eclipse-workspace/TrailerTrash
ECLIPSE_TRAILEREXE_BASE=$(PWD)/eclipse-workspace/trailer
ECLIPSE_TTEXE_BASE=$(PWD)/eclipse-workspace/tt
ECLIPSE_TRAILER_TEST_BASE=$(PWD)/eclipse-workspace/TrailerTest

eclipse:
	mkdir -p $(ECLIPSE_TRAILER_BASE)
	cat eclipse-template/TrailerTrash/cproject | java -cp replacer Replacer "--TRAILER_BASE--" $(PWD) > $(ECLIPSE_TRAILER_BASE)/.cproject
	cat eclipse-template/TrailerTrash/project | java -cp replacer Replacer "--TRAILER_BASE--"  $(PWD) > $(ECLIPSE_TRAILER_BASE)/.project
	mkdir -p $(ECLIPSE_TRAILEREXE_BASE)
	cat eclipse-template/trailer/cproject | java -cp replacer Replacer "--TRAILER_BASE--" $(PWD) > $(ECLIPSE_TRAILEREXE_BASE)/.cproject
	cat eclipse-template/trailer/project | java -cp replacer Replacer "--TRAILER_BASE--"  $(PWD) > $(ECLIPSE_TRAILEREXE_BASE)/.project
	mkdir -p $(ECLIPSE_TTEXE_BASE)
	cat eclipse-template/tt/cproject | java -cp replacer Replacer "--TRAILER_BASE--" $(PWD) > $(ECLIPSE_TTEXE_BASE)/.cproject
	cat eclipse-template/tt/project | java -cp replacer Replacer "--TRAILER_BASE--"  $(PWD) > $(ECLIPSE_TTEXE_BASE)/.project
	mkdir -p $(ECLIPSE_TRAILER_TEST_BASE)
	cat eclipse-template/TrailerTest/cproject | java -cp replacer Replacer "--TRAILER_BASE--" $(PWD) > $(ECLIPSE_TRAILER_TEST_BASE)/.cproject
	cat eclipse-template/TrailerTest/project | java -cp replacer Replacer "--TRAILER_BASE--"  $(PWD) > $(ECLIPSE_TRAILER_TEST_BASE)/.project



