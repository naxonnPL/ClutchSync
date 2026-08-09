# **ClutchSync**



A light C++ application that integrates **Counter-Strike 2** with the **Spotify API**. Program automatically controls music playback based on the current game state (Game State Integration).



#### **Features:**

* **🎧Lobby** - Seamless playback of your selected track while browsing the main menu.
* **🛒** **Buy Time** - Automatically lowers volume so you can focus on the team callouts.
* **🔇Round Mute** - Instantly pauses music as soon as the round starts.
* **🏆** **MVP Kit** - Triggers a designated track at specific moment when you earn MVP reward



**-------------------------------------------------------------------------------------------------------------**



#### **🖥️Requirements:**

* **Spotify Premium account** to access Spotify Web API for playback control
* **Active Spotify Client** running in the background (e.g.,Spotify Desktop App)



**-------------------------------------------------------------------------------------------------------------**



#### **🔐Setup Guide:**

##### **For: token.json**

###### **Create a Spotify Developer App:**

* Navigate to the \[**Spotify Developer Dashboard](**https://developer.spotify.com/dashboard) and sign in.
* Click **Create App (Web API)**
* In your app's **settings**, add the following URL under **Redirect URIs:**

&#x20;      \[http://127.0.0.1:3000/callback]http://127.0.0.1:3000/callback

4.- Save the changes and copy your **Client ID** and **Client Secret**



###### **Generate Access \& Refresh Tokens:**

* Open the following URL in your browser (replace '**YOUR\_CLIENT\_ID'** with your actual Client ID):



**https://accounts.spotify.com/authorize?response\_type=code\&client\_id=YOUR\_CLIENT\_ID\&scope=user-modify-playback-state%20user-read-playback-state\&redirect\_uri=http://127.0.0.1:3000/callback**



* Log in to Spotify and click **Agree.**



**3. Extract AUTHORIZATION CODE:** You will be redirected to an unreachable page (This site can't be reached). Look at your browser's address bar and copy the full string after **code=** to the end.

* Paste **CLIENT\_ID** and **CLIENT\_SECRET** code to **token.json** (**1st** and **2nd** row)
* Open **PowerShell** and run the following command (replace parameters with your actual values).



**$auth = \[Convert]::ToBase64String(\[Text.Encoding]::ASCII.GetBytes("YOUR\_CLIENT\_ID:YOUR\_CLIENT\_SECRET")); Invoke-RestMethod -Uri "https://accounts.spotify.com/api/token" -Method Post -Headers @{ Authorization = "Basic $auth"; "Content-Type" = "application/x-www-form-urlencoded" } -Body @{ grant\_type = "authorization\_code"; code = "YOUR\_AUTHORIZATION\_CODE"; redirect\_uri = "http://127.0.0.1:3000/callback" } | ConvertTo-Json**



You should get similar output:


**{**

&#x20;   **"access\_token":  "here\_is\_your\_code - paste it in spotify\_token in token.json file",**

&#x20;   **"token\_type":  "Bearer",**

&#x20;   **"expires\_in":  3600,**

&#x20;   **"refresh\_token":  "here\_is\_your\_code - paste it in refresh\_token in token.json file",**

&#x20;   **"scope":  "user-modify-playback-state user-read-playback-state"**

**}**



##### **For: gamestate\_integration\_ClutchSync.cfg**

Drag **gamestate\_integration\_ClutchSync.cfg** file from the program folder to this path:

**C:\\Program Files (x86)\\Steam\\steamapps\\common\\Counter-Strike Global Offensive\\game\\csgo\\cfg\\DROP\_THE\_FILE\_IN\_THIS\_FOLDER**

This is required for the program to read game events.



##### **For: settings.json**

**For now, there is only one slot to put chosen music in (tracks -> uri** and **mvp -> uri).**

**start\_seconds**, **volume, fading** and **duration\_seconds** are pretty self explanatory



##### **How to get Spotify URIs:**

To configure custom tracks or playlists for specific events, you need to use **Spotify URIs** (example: **spotify:track:6zfT9uWmfX4YVXq3MU93dH**)



1. **Right-click** any track
2. Select **Share -> Copy link**
3. You will obtain a link in your clipboard, like: **https://open.spotify.com/track/6zfT9uWmfX4YVXq3MU93dH?si=123456...**
4. Take ID between '**/track/**' and **'?' (e.g., 6zfT9uWmfX4YVXq3MU93dH)** and add '**spotify:track:**'
5. It should look like this: **spotify:track:6zfT9uWmfX4YVXq3MU93dH**





















