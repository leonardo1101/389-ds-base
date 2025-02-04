RPMBUILD ?= $(PWD)/rpmbuild
RPM_VERSION ?= $(shell $(PWD)/rpm/rpmverrel.sh version)
RPM_RELEASE ?= $(shell $(PWD)/rpm/rpmverrel.sh release)
PACKAGE = 389-ds-base
RPM_NAME_VERSION = $(PACKAGE)-$(RPM_VERSION)
TARBALL = $(RPM_NAME_VERSION).tar.bz2
NUNC_STANS_URL ?= $(shell rpmspec -P -D 'use_nunc_stans 1' $(RPMBUILD)/SPECS/389-ds-base.spec | awk '/^Source4:/ {print $$2}')
NUNC_STANS_TARBALL ?= $(shell basename "$(NUNC_STANS_URL)")
JEMALLOC_URL ?= $(shell rpmspec -P $(RPMBUILD)/SPECS/389-ds-base.spec | awk '/^Source3:/ {print $$2}')
JEMALLOC_TARBALL ?= $(shell basename "$(JEMALLOC_URL)")
NUNC_STANS_ON = 1
BUNDLE_JEMALLOC = 0

clean:
	rm -rf dist
	rm -rf rpmbuild

local-archive:
	-mkdir -p dist/$(RPM_NAME_VERSION)
	rsync -a --exclude=dist --exclude=.git --exclude=rpmbuild . dist/$(RPM_NAME_VERSION)

tarballs: local-archive
	-mkdir -p dist/sources
	cd dist; tar cfj sources/$(TARBALL) $(RPM_NAME_VERSION)
	rm -rf dist/$(RPM_NAME_VERSION)
	cd dist/sources ; \
	if [ $(NUNC_STANS_ON) -eq 1 ]; then \
	    wget $(NUNC_STANS_URL) ; \
	fi ; \
	if [ $(BUNDLE_JEMALLOC) -eq 1 ]; then \
	    wget $(JEMALLOC_URL) ; \
	fi

rpmroot:
	rm -rf $(RPMBUILD)
	mkdir -p $(RPMBUILD)/BUILD
	mkdir -p $(RPMBUILD)/RPMS
	mkdir -p $(RPMBUILD)/SOURCES
	mkdir -p $(RPMBUILD)/SPECS
	mkdir -p $(RPMBUILD)/SRPMS
	sed -e s/__VERSION__/$(RPM_VERSION)/ -e s/__RELEASE__/$(RPM_RELEASE)/ \
	-e s/__NUNC_STANS_ON__/$(NUNC_STANS_ON)/ \
	-e s/__BUNDLE_JEMALLOC__/$(BUNDLE_JEMALLOC)/ \
	rpm/$(PACKAGE).spec.in > $(RPMBUILD)/SPECS/$(PACKAGE).spec

rpmdistdir:
	mkdir -p dist/rpms

srpmdistdir:
	mkdir -p dist/srpms

rpmbuildprep:
	cp dist/sources/$(TARBALL) $(RPMBUILD)/SOURCES/
	if [ $(NUNC_STANS_ON) -eq 1 ]; then \
		cp dist/sources/$(NUNC_STANS_TARBALL) $(RPMBUILD)/SOURCES/ ; \
	fi
	if [ $(BUNDLE_JEMALLOC) -eq 1 ]; then \
		cp dist/sources/$(JEMALLOC_TARBALL) $(RPMBUILD)/SOURCES/ ; \
	fi
	cp rpm/$(PACKAGE)-* $(RPMBUILD)/SOURCES/


srpms: rpmroot srpmdistdir tarballs rpmbuildprep
	rpmbuild --define "_topdir $(RPMBUILD)" -bs $(RPMBUILD)/SPECS/$(PACKAGE).spec
	cp $(RPMBUILD)/SRPMS/$(RPM_NAME_VERSION)-*.src.rpm dist/srpms/
	rm -rf $(RPMBUILD)

patch_srpms: rpmroot srpmdistdir tarballs rpmbuildprep
	cp rpm/*.patch $(RPMBUILD)/SOURCES/
	rpm/add_patches.sh rpm $(RPMBUILD)/SPECS/$(PACKAGE).spec
	rpmbuild --define "_topdir $(RPMBUILD)" -bs $(RPMBUILD)/SPECS/$(PACKAGE).spec
	cp $(RPMBUILD)/SRPMS/$(RPM_NAME_VERSION)-*.src.rpm dist/srpms/
	rm -rf $(RPMBUILD)

rpms: rpmroot srpmdistdir rpmdistdir tarballs rpmbuildprep
	rpmbuild --define "_topdir $(RPMBUILD)" -ba $(RPMBUILD)/SPECS/$(PACKAGE).spec
	cp $(RPMBUILD)/RPMS/*/$(RPM_NAME_VERSION)-*.rpm dist/rpms/
	cp $(RPMBUILD)/RPMS/*/$(PACKAGE)-*-$(RPM_VERSION)-*.rpm dist/rpms/
	cp $(RPMBUILD)/SRPMS/$(RPM_NAME_VERSION)-*.src.rpm dist/srpms/
	rm -rf $(RPMBUILD)

patch_rpms: rpmroot srpmdistdir rpmdistdir tarballs rpmbuildprep
	cp rpm/*.patch $(RPMBUILD)/SOURCES/
	rpm/add_patches.sh rpm $(RPMBUILD)/SPECS/$(PACKAGE).spec
	rpmbuild --define "_topdir $(RPMBUILD)" -ba $(RPMBUILD)/SPECS/$(PACKAGE).spec
	cp $(RPMBUILD)/RPMS/*/$(RPM_NAME_VERSION)-*.rpm dist/rpms/
	cp $(RPMBUILD)/RPMS/*/$(PACKAGE)-*-$(RPM_VERSION)-*.rpm dist/rpms/
	cp $(RPMBUILD)/SRPMS/$(RPM_NAME_VERSION)-*.src.rpm dist/srpms/
	rm -rf $(RPMBUILD)
