all:
	$(CCS_ECLIPSE) -noSplash -data $(OPEN_PRU_PATH)/ccs_projects -application com.ti.ccstudio.apps.projectBuild -ccs.projects $(PROJECT_NAME) -ccs.configuration $(PROFILE)

clean:
	$(CCS_ECLIPSE) -noSplash -data $(OPEN_PRU_PATH)/ccs_projects -application com.ti.ccstudio.apps.projectBuild -ccs.projects $(PROJECT_NAME) -ccs.configuration $(PROFILE) -ccs.clean

export:
	$(MKDIR) $(OPEN_PRU_PATH)/ccs_projects
	$(CCS_ECLIPSE) -noSplash -data $(OPEN_PRU_PATH)/ccs_projects -application com.ti.ccstudio.apps.projectCreate -ccs.projectSpec example.projectspec -ccs.overwrite full
