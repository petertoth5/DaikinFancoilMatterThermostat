Goal of the project: Develop a matter thermostat that can interface the Daikin thermostat (EKWHCTRL1) and the PCB in the fancoil (INN-FR-B32) via modbus. 
This matter thermostats shall be visible to matter controllers and hubs like the Aeotec smartthings hub or Apple TV 4K or any matter compatilbe hub. The new thermostat shall be a matter device over wifi.
More detials will come during the development. This is just a brief of the scope.

Hardware: The Daikin devices are the Daikin thermostat (EKWHCTRL1) and the PCB in the fancoil (INN-FR-B32) with modbus connection. The matter thermostat shall be implemented on an ESPS3 device. Specifically this board will be used: ESP32-S3-WROOM-1-N16R8-M
There will be RS485 transceivers. This type: MAX485-M
There is one thermostat and one fan coil on the bus currently. They are directly connected to each other. The new thermostat shall be added to the bus, without interrupting the existing connection and communication.
SW architecture: Now just talk about the dev environment, not about the SW architecture itself. Since the device we are working with is an ESP32S3 device the dev env shall be ESP-IDF. All modules what will be used shall be used through the IDF component manager (https://pypi.org/project/idf-component-manager/). Just modules available in the ESP component registry (https://components.espressif.com/) allowed to be used. Further details like SW layers, connection and deep SW architecture will be figured out during the next steps. No scheduling is in scope now, it will also come later. Preserve this info and we will do /brainstorming during the upcoming sessions. 

Now prepare the dev environment based on these and /capture know how to wiki, update the .md files with the necessary information.

After this is finished I want to design the agents I want to use during the development and design. Next step shall be agent definition. Parallel to agent definition we might identify new skills and rule. But next step shall be agent definition.
