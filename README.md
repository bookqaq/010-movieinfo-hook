## DISCLAIMER

Although I've implemented most of the hooks myself, this project is modified from [2dxcamhook](https://github.com/aixxe/2dxcamhook), which is the perfect one for me to learn writing hooks. 

## 010-movieinfo-hook

A hook that send necessary IIDX\<version\>music.movieinfo data to [010-record-api](https://github.com/bookqaq/010-record-api) version 1.1.0+

This is preferred to be used if you are an operator of a cabient / "pc that run the game + controller", use 010-record-api as WebAPI2 server, and want to differ video files uploaded by one player from another. No modification is needed for xrpc server.

### Why?

There is a missing handle in video purchase and upload procedure for 010-record-api, which is `IIDX<version>music.movieinfo`. This request data can't be handled by it unless some xrpc server send it actively. 

### Compatibility

For now, hook offsets are found manually by me. So it might not be a complete match for all versions of IIDX supported by 010-record-api.

- beatmania IIDX 31 EPOLIS (010-2024082600)

### Configuration

#### 1. Download latest version of 010-record-api(1.1.0+)

Get it from releases: https://github.com/bookqaq/010-record-api/releases

#### 2. Generate a new config.toml

Delete your existing `config.toml` or put it elsewhere (if exists), and run `010-record-api.exe` to generate a new one.

#### 3. Edit config file to enable feature

Open `config.toml`. Set `feature_xrpcIIDXMusicMovieInfo` to `true`

#### 4. Install hook

Compile from source or download a pre-built version from the [releases](https://github.com/bookqaq/010-movieinfo-hook/releases/)

Copy the `010-movieinfo-hook.dll` to your game directory

Alter your launch command to load the library during startup

##### Bemanitools

```
launcher.exe [...] -K 010-movieinfo-hook.dll
```

##### spice2x

```
spice64.exe [...] -k 010-movieinfo-hook.dll
```

#### 5. Start the server and your game

Start 010-record-api.exe, and maybe start your xrpc server. Run your game afterwards.

You could check hook's log in avs2::log with tag "010-movieinfo-hook".