
# List all the sub-projects
SUBDIRS = rt11dsk Sav2Cartridge sav2wav SavDisasm UkncArcMan

.PHONY: subdirs $(SUBDIRS)

subdirs: $(SUBDIRS)

$(SUBDIRS): FORCE
	$(MAKE) -C $@

# A target without prerequisites and a recipe, and there is no file named `FORCE`.
# `make` will always run this and any other target that depends on it.
FORCE:
