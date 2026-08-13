# **ClutchSync** (Beta)
Developed by [naxonn](https://github.com/naxonnPL)

A light C++ application that integrates **Counter-Strike 2** with the **Spotify API**. Program automatically controls music playback based on the current game state (Game State Integration).

### 🛠️ Built with Dependencies
* [nlohmann/json](https://github.com/nlohmann/json) - JSON for Modern C++ by Niels Lohmann (MIT License)
* [cpp-httplib](https://github.com/yhirose/cpp-httplib) - A C++ header-only HTTP/HTTPS server and client library by Yuji Hirose (MIT License)

---

#### **Features**
* **🎧 Lobby** - Seamless playback of your selected track while browsing the main menu.
* **🛒 Buy Time** - Automatically lowers volume so you can focus on the team callouts.
* **🔇 Round Mute** - Instantly pauses music as soon as the round starts.
* **🏆 MVP Kit** - Triggers a designated track at specific moment when you earn an MVP reward.
* **🚀 Round Start** - Plays your chosen track for the first few seconds after the round starts.
* **⚙️ Customization** - Fully configurable: set custom tracks, start positions, playback duration, and volume levels.

---

#### **🖥️ Requirements**
* **Spotify Premium account** to access Spotify Web API for playback control.
* **Active Spotify Client** running in the background (e.g.,Spotify Desktop App).

---

#### **🔐 Setup Guide**

##### **For: token.json**

###### **Create a Spotify Developer App:**
* Navigate to the [Spotify Developer Dashboard](https://developer.spotify.com/dashboard) and sign in.
* Click **Create App (Web API)**
* In your app's **settings**, add the following URL under **Redirect URIs:**
`http://127.0.0.1:3000/callback`
* Save the changes and copy your **Client ID** and **Client Secret**

###### **Generate Access & Refresh Tokens:**
* Open the following URL in your browser (replace `YOUR_CLIENT_ID` with your actual Client ID):
`https://accounts.spotify.com/authorize?response_type=code&client_id=YOUR_CLIENT_ID&scope=user-modify-playback-state%20user-read-playback-state&redirect_uri=http://127.0.0.1:3000/callback`
* Log in to Spotify and click **Agree.**
* Extract **AUTHORIZATION CODE:** You will be redirected to an unreachable page (This site can't be reached). Look at your browser's address bar and copy the full string after `code=` to the end of the URL.
* Paste **CLIENT_ID** and **CLIENT_SECRET** code into **token.json** (**1st** and **2nd** row)
* Open **PowerShell** and run the following command (replace parameters with your actual values).
```powershell
$auth = [Convert]::ToBase64String([Text.Encoding]::ASCII.GetBytes("YOUR_CLIENT_ID:YOUR_CLIENT_SECRET")); Invoke-RestMethod -Uri "https://accounts.spotify.com/api/token" -Method Post -Headers @{ Authorization = "Basic $auth"; "Content-Type" = "application/x-www-form-urlencoded" } -Body @{ grant_type = "authorization_code"; code = "YOUR_AUTHORIZATION_CODE"; redirect_uri = "http://127.0.0.1:3000/callback" } | ConvertTo-Json
```

You should get similar JSON output:
```json
{
  "access_token": "here_is_your_code - paste it in spotify_token in token.json file",
  "token_type": "Bearer",
  "expires_in": 3600,
  "refresh_token": "here_is_your_code - paste it in refresh_token in token.json file",
  "scope": "user-modify-playback-state user-read-playback-state"
}
```
Copy these vaules into your `token.json` file.

##### **For: gamestate_integration_ClutchSync.cfg**

* Drag **gamestate_integration_ClutchSync.cfg** file from the program folder to this path:
  `C:\Program Files (x86)\Steam\steamapps\common\Counter-Strike Global Offensive\game\csgo\cfg\DROP_THE_FILE_IN_THIS_FOLDER`
This is required for the program to read game events.

##### **For: settings.json**

* **`start_seconds`** - Timestamp in seconds where playback should begin.
* **`duration_seconds`** - How long the track should play before stopping (recommended default setting).
* **`volume`** - Percentage for specific game states.
* **`fading_duration_ms`** - Audio transition time in milliseconds (recommended default setting).
  
##### **How to get Spotify URIs:**

To configure custom tracks or playlists for specific events, you need to use **Spotify URIs** (e.g, `spotify:track:6zfT9uWmfX4YVXq3MU93dH`)
* **Right-click** any track. Select **Share -> Copy link**
* You will obtain a link in your clipboard, like: **https://open.spotify.com/track/01234567890ABCDEFGHIJK...**
* Take the ID between `/track/` and `?` (e.g., `01234567890ABCDEFGHIJK`)** and prefix it with `spotify:track:`
* It should look like this: `spotify:track:01234567890ABCDEFGHIJK`.

Have fun!
