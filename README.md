## $\text{\color{lightgreen}Client install}$
To connect to the server you need two client parts ([Http](https://github.com/semachkin/HttpRobloxServer/tree/master?tab=readme-ov-file#textcoloryellowto-install-http-client) and [Rbx](https://github.com/semachkin/HttpRobloxServer/tree/master?tab=readme-ov-file#textcoloryellowto-install-rbx-client) client)
##
### $\text{\color{yellow}To install Http client}$
Download [rbxmx file](https://github.com/semachkin/HttpRobloxServer/releases/download/HttpClient/httpclient.rbxmx) and put it in ServerScriptService
##
### $\text{\color{yellow}To install Rbx client}$
1. go to `C:\Users\user\AppData\Local\Roblox\Versions\Current Version` or right click on the Roblox Studio shortcut on your desktop and select the file location
2. find or create `ExtraContent\scripts\PlayerScripts\StarterPlayerScripts`
3. unpack [RbxClient.7z](https://github.com/semachkin/HttpRobloxServer/releases/download/RbxClient/RbxClient.7z) into this folder without creating subfolders

##
 - ##### To connect to the server, specify the host's proxy IP or private net IP address and server port in the SERVER_CONFIG attributes
 - ##### exchange_rate attribute usually does not exceed 8.3 requests per second. If you want more download [Roblox Mod Manager](https://github.com/MaximumADHD/Roblox-Studio-Mod-Manager) and increase fflag DFIntUserHttpRequestsPerMinuteLimit
- ##### ⚠️ For now, instance replication only works fine on R6 avatars

## $\text{\color{lightgreen}Server build }$
Make sure that the [GNU Make](https://www.gnu.org/software/make/) utility is installed and run build.bat

#### $\text{\color{red}Contact support on Discord semachkaredflag }$
