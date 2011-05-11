TARGETS = skye_client skye_server
DIRS	= common client server util test

all: $(TARGETS) util

$(TARGETS) : common

common : force_look
	@cd common; make

skye_client : force_look
	@cd client; make

skye_server : force_look
	@cd server; make

util : force_look
	@cd util; make

test : force_look
	@cd test; make

clean :
	@for d in $(DIRS); do (cd $$d; $(MAKE) clean ); done

tags : force_look
	ctags -R ./ /usr/include/rpc/ /usr/include/fuse/ /usr/local/include/pvfs2*

git-ignored :
	git ls-files --others -i --exclude-standard

deploy :
	rsync -r --exclude .git --delete ./ oc:skyefs

force_look :
	@true
