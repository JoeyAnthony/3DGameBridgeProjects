#cd ./files
#Start-Sleep -Seconds 1
#start-process -filepath inject64.exe eldenring.exe

#Start-Process -filepath "D:\SteamLibrary\steamapps\common\Monster Hunter World\MonsterHunterWorld.exe"
#Start-Process -filepath explorer.exe shell:appsFolder\Microsoft.ForzaHorizon4Demo_1.192.906.2_x64__8wekyb3d8bbwe!App
$tries = 0;
while($tries -lt 20){
#while(1){	
	$inject64proc = Get-Process inject64 -ErrorAction SilentlyContinue
	if(!$inject64proc){
		./srReshade_Inject.exe "P5R.exe"
		$tries++;
	}
}