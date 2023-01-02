adb shell am force-stop com.beatgames.beatsaber
adb shell am start com.beatgames.beatsaber/com.unity3d.player.UnityPlayerActivity
start-sleep 1
adb shell am start com.beatgames.beatsaber/com.unity3d.player.UnityPlayerActivity | out-null
