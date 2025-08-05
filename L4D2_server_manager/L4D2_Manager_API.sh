#!/bin/bash

# =================================================================
# L4D2 ���������������� API �汾 (�ǽ���ʽ)
# �ı���: L4D2_Manager.sh 2627 - Polaris
# ����: ΪWeb����ṩ���ִ�нӿ�
# =================================================================

# --- Bash �汾��� ---
if ((BASH_VERSINFO[0] < 4)); then
    echo "����: �˽ű���Ҫ Bash 4.0 ����߰汾�������С�" 
    exit 1
fi

# ########################## �û������� ##########################
ServerRoot="/home/steam/l4d2server" #����֮·�������ļ���Ŀ¼
SteamCMDDir="/home/steam/steamcmd" #steamcmd��Ŀ¼
STEAM_USER=""
STEAM_PASSWORD=""
declare -A ServerInstances=(
    ["����_ս��"]="
        Port=27015
        HostName='[CN] My L4D2 Campaign Server'
        MaxPlayers=8
        StartMap='c1m1_hotel'
        ExtraParams='+sv_gametypes \"coop,realism,survival\"'
    "
    ["����_�Կ�"]="
        Port=27016
        HostName='[CN] My L4D2 Versus Server'
        MaxPlayers=8
        StartMap='c5m1_waterfront'
        ExtraParams='+sv_gametypes \"versus,teamversus,scavenge\"'
    "
)
# ��������
REQUIRED_PACKAGES=(
    "screen"           # ���ڹ��������ʵ���Ự
    "wget"             # ��������SteamCMD�Ͳ��
    "rsync"            # ���ڲ���ļ�ͬ��
    "tar"              # ���ڽ�ѹ��װ��
    "gzip"             # ���ڴ���ѹ���ļ�
    "lib32gcc-s1"     # 32λ����ʱ����
    "lib32stdc++6"     # 32λC++��
    "expect"
)

# #################################################################


# --- �ű��������� ---
L4d2Dir="$ServerRoot/left4dead2"
ScriptDir="$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
PluginSourceDir="$ScriptDir/Available_Plugins"
ReceiptsDir="$ScriptDir/Installed_Receipts"
InstallerDir="$ScriptDir/SourceMod_Installers"��

# ȷ��Ŀ¼����
mkdir -p "$PluginSourceDir" "$ReceiptsDir" "$InstallerDir" "$ServerRoot"

# --- ���Ĺ��ܺ��� ---
function Check-And-Install-Dependencies() {
    echo "--- ��ʼ���ϵͳ���� ---"
    
    # ����������
    if command -v apt &> /dev/null; then
        PACKAGE_MANAGER="apt"
        UPDATE_CMD="apt update -y"
        INSTALL_CMD="apt install -y"
    elif command -v yum &> /dev/null; then
        PACKAGE_MANAGER="yum"
        UPDATE_CMD="yum check-update"
        INSTALL_CMD="yum install -y"
    else
        echo "����: δ�ҵ�֧�ֵİ������� (apt/yum)"
        return 1
    fi

    # ���°��б�
    echo "���ڸ��°��б�..."
    eval "$UPDATE_CMD" || {
        echo "����: ���°��б�ʧ�ܣ����Լ�����װ..."
    }

    # ��鲢��װȱʧ������
    local missing_pkgs=()
    for pkg in "${REQUIRED_PACKAGES[@]}"; do
        if ! command -v "$pkg" &> /dev/null && ! dpkg -s "$pkg" &> /dev/null; then
            missing_pkgs+=("$pkg")
        fi
    done

    if [ ${#missing_pkgs[@]} -eq 0 ]; then
        echo "���б�Ҫ�����Ѱ�װ"
        return 0
    fi

    # ��װȱʧ������
    echo "��Ҫ��װ��������: ${missing_pkgs[*]}"
    eval "$INSTALL_CMD ${missing_pkgs[*]}" || {
        echo "����: ��װ����ʧ��"
        return 1
    }

    echo "--- ��������밲װ��� ---"
    return 0
}


function Deploy-L4D2Server_NonInteractive() {
    echo "--- ��ʼ����/���� L4D2 ������ ---"
    local steamcmd_executable="$SteamCMDDir/steamcmd.sh"

    if [ ! -f "$steamcmd_executable" ]; then
        echo "δ�ҵ� SteamCMD�������Զ�����..."
        mkdir -p "$SteamCMDDir"
        wget -O "$SteamCMDDir/steamcmd_linux.tar.gz" "https://steamcdn-a.akamaihd.net/client/installer/steamcmd_linux.tar.gz"
        tar -xvzf "$SteamCMDDir/steamcmd_linux.tar.gz" -C "$SteamCMDDir"
        rm "$SteamCMDDir/steamcmd_linux.tar.gz"
    fi

    # ����ʹ�û��������е��û���������
    if [ -n "$STEAM_USER" ] && [ -n "$STEAM_PASSWORD" ]; then
        echo "ʹ���˻�: $STEAM_USER"
        unbuffer "$steamcmd_executable" +force_install_dir "$ServerRoot" +login "$STEAM_USER" "$STEAM_PASSWORD" +app_update 222860 validate +quit | sed -u 's/\x1b\[[0-9;]*[a-zA-Z]//g'
    else
        # ����ʹ��������¼
        echo "ʹ������ (anonymous) ��¼��"
        unbuffer "$steamcmd_executable" +force_install_dir "$ServerRoot" +login anonymous +app_update 222860 validate +quit | sed -u 's/\x1b\[[0-9;]*[a-zA-Z]//g'
    fi
    
    if [ ${PIPESTATUS[0]} -eq 0 ]; then
        echo "--- L4D2 �������ļ�����/���³ɹ�! ---"
    else
        echo "--- SteamCMD ִ��ʧ�ܣ�����������־�� ---" >&2
        return 1
    fi
}

function Start-L4D2ServerInstance_NonInteractive() {
    local instanceName="$1"
    if [ ! -v "ServerInstances[$instanceName]" ]; then
        echo "����: δ���������ҵ���Ϊ '$instanceName' ��ʵ����" >&2
        exit 1
    fi
    eval "${ServerInstances[$instanceName]}" # ����ʵ������

    local session_name="l4d2_manager_${instanceName}"
    if screen -ls | grep -q "\.$session_name"; then
        echo "����: ��Ϊ '$session_name' �� screen �Ự�������С�" >&2
        exit 1
    fi

    local srcds_path="./srcds_run"
    local srcdsArgs="-console -game left4dead2 -port $Port +maxplayers $MaxPlayers +map $StartMap +hostname \"$HostName\" $ExtraParams"

    echo "׼���� Screen �Ự '$session_name' ������������..."
    echo "ִ������: $srcds_path $srcdsArgs"
    (cd "$ServerRoot" && screen -dmS "$session_name" $srcds_path $srcdsArgs)
    sleep 1
    if screen -ls | grep -q "\.$session_name"; then
        echo "�ɹ�! ʵ�� '$instanceName' ���� screen �Ự '$session_name' ��������"
    else
        echo "����: ����������ʧ�ܡ����� screen �ͷ������ļ��Ƿ�������" >&2
        exit 1
    fi
}

function Stop-L4D2ServerInstance_NonInteractive() {
    local instanceName="$1"
    local session_name="l4d2_manager_${instanceName}"
    if ! screen -ls | grep -q "\.$session_name"; then
        echo "����: δ�ҵ���Ϊ '$session_name' �� screen �Ự��" >&2
        exit 1
    fi
    echo "������Ự '$session_name' ���� quit ָ��..."
    screen -S "$session_name" -X stuff $'quit\n'
    echo "�ɹ�! ָֹͣ���ѷ�����ʵ�� '$instanceName'��"
}

function Invoke-PluginInstallation() {
    local pluginName="$1"
    local pluginPath="$PluginSourceDir/$pluginName"
    local receiptPath="$ReceiptsDir/$pluginName.receipt"
    if [ ! -d "$pluginPath" ]; then
      echo "����: �Ҳ������ԴĿ¼ '$pluginPath'��" >&2
      exit 1
    fi
    echo "--- ��ʼ��װ��� '$pluginName' ---"
    echo "�����ļ��嵥..."
    (cd "$pluginPath" && find . -type f | sed 's|^\./||') > "$receiptPath"
    if [ $? -ne 0 ]; then
        echo "����! ��������嵥 '$receiptPath' ʧ�ܡ�" >&2
        rm -f "$receiptPath"
        exit 1
    fi
    
    echo "���ڽ��ļ��ƶ���������Ŀ¼..."
    rsync -a "$pluginPath/" "$ServerRoot/"
    
    if [ $? -eq 0 ]; then
        # �ɹ�֮��ɾ��ԴĿ¼
        rm -rf "$pluginPath"
        echo "--- ��� '$pluginName' ��װ�ɹ�! ---"
    else
        echo "--- ����! ��װ��� '$pluginName' ʱ�ļ�����ʧ�ܡ� ---" >&2
        rm -f "$receiptPath"
        exit 1
    fi
}

function Invoke-PluginUninstallation() {
    local pluginName="$1"
    local receiptPath="$ReceiptsDir/$pluginName.receipt"
    if [ ! -f "$receiptPath" ]; then
        echo "����: �Ҳ�������嵥 '$receiptPath'���޷�ж�ء�" >&2
        exit 1
    fi
    
    echo "--- ��ʼж�ز�� '$pluginName' ---"
    
    local pluginReclaimFolder="$PluginSourceDir/$pluginName"
    mkdir -p "$pluginReclaimFolder"

    while IFS= read -r relativePath || [[ -n "$relativePath" ]]; do
        [ -z "$relativePath" ] && continue

        local serverFile="$ServerRoot/$relativePath"
        local destinationFile="$pluginReclaimFolder/$relativePath"
        
        if [ -f "$serverFile" ]; then
            # ȷ��Ŀ��Ŀ¼����
            mkdir -p "$(dirname "$destinationFile")"
            # �ƶ��ļ�
            mv "$serverFile" "$destinationFile"
        fi
    done < "$receiptPath"

    # ����������Ͽ��ܲ����Ŀ�Ŀ¼
    while IFS= read -r relativePath; do
        [ -z "$relativePath" ] && continue
        local dirOnServer="$ServerRoot/$(dirname "$relativePath")"
        if [ -d "$dirOnServer" ] && [ -z "$(ls -A "$dirOnServer")" ]; then
            rmdir "$dirOnServer" 2>/dev/null
        fi
    done < <(sort -r "$receiptPath")

    rm -f "$receiptPath"
    echo "--- ��� '$pluginName' ж�سɹ�! ---"
}

function Install-SourceModAndMetaMod_NonInteractive() {
    echo "--- ��ʼ��װ SourceMod & MetaMod ---"
    mkdir -p "$InstallerDir"
    
    local metamod_tar
    metamod_tar=$(find "$InstallerDir" -maxdepth 1 -name "mmsource-*.tar.gz" | sort -V | tail -n 1)
    
    if [ -n "$metamod_tar" ]; then
        echo "���� MetaMod: $(basename "$metamod_tar")"
        if tar -xzf "$metamod_tar" -C "$L4d2Dir"; then
            echo "MetaMod ��ѹ��ɡ�"
            mkdir -p "$L4d2Dir/addons"
            echo -e "\"Plugin\"\n{\n\t\"file\"\t\"addons/metamod/bin/server\"\n}" > "$L4d2Dir/addons/metamod.vdf"
            echo "'metamod.vdf' �����ɹ���"
        else
            echo "����: ��ѹ MetaMod ʱ����" >&2
            return 1
        fi
    else
        echo "����: �� '$InstallerDir' ��δ�ҵ� MetaMod ��װ�� (mmsource-*.tar.gz)��" >&2
    fi
    
    local sourcemod_tar
    sourcemod_tar=$(find "$InstallerDir" -maxdepth 1 -name "sourcemod-*.tar.gz" | sort -V | tail -n 1)

    if [ -n "$sourcemod_tar" ]; then
        echo "���� SourceMod: $(basename "$sourcemod_tar")"
        if tar -xzf "$sourcemod_tar" -C "$L4d2Dir"; then
            echo "SourceMod ��ѹ��ɡ�"
        else
            echo "����: ��ѹ SourceMod ʱ����" >&2
            return 1
        fi
    else
        echo "����: �� '$InstallerDir' ��δ�ҵ� SourceMod ��װ�� (sourcemod-*.tar.gz)��" >&2
    fi
    
    echo "--- ��װ����ִ����� ---"
}

function Resolve-Path() {
    local path="$1"
    local base_dir
    local relative_path

    if [[ "$path" == "plugin_dir" || "$path" == "plugin_dir/"* ]]; then
        base_dir="$PluginSourceDir"
        if [[ "$path" == "plugin_dir" ]]; then
            relative_path=""
        else
            relative_path=${path#plugin_dir/}
        fi
    elif [[ "$path" == "installer_dir" || "$path" == "installer_dir/"* ]]; then
        base_dir="$InstallerDir"
        if [[ "$path" == "installer_dir" ]]; then
            relative_path=""
        else
            relative_path=${path#installer_dir/}
        fi
    else
        base_dir="$ServerRoot"
        relative_path="$path"
    fi

    local target_path="$base_dir/$relative_path"
    
    # ����Ϊ���ԡ��淶��·��
    REAL_TARGET_PATH=$(realpath -m "$target_path")
    REAL_BASE_DIR=$(realpath -m "$base_dir")

    # ��ȫ�Լ��
    if [[ "$REAL_TARGET_PATH" != "$REAL_BASE_DIR"* ]]; then
        echo "����: ��Ч��Σ�յ�·������ '$path'" >&2
        return 1
    fi
    return 0
}

# --- JSON���ɸ������� ---
json_escape() {
    printf '%s' "$1" | python3 -c 'import json,sys; print(json.dumps(sys.stdin.read()))'
}

# --- API ������� ---
ACTION="$1"
shift

case "$ACTION" in
    check_deps)
        Check-And-Install-Dependencies
        ;;

    get_status)
        server_deployed="false"
        sm_installed="false"
	mm_installed="false"
        if [ -f "$ServerRoot/srcds_run" ]; then server_deployed="true"; fi
        if [ -f "$L4d2Dir/addons/metamod.vdf" ] ; then mm_installed="true"; fi
	if  [ -f "$L4d2Dir/addons/sourcemod/bin/sourcemod_mm_i486.so" ]; then sm_installed="true"; fi
        echo "{\"serverDeployed\": $server_deployed, \"smInstalled\": $sm_installed, \"mmInstalled\": $mm_installed}"
        ;;

    get_instances)
        json="{"
        first=true
        for name in "${!ServerInstances[@]}"; do
            session_name="l4d2_manager_${name}"
            running="false"
            if screen -ls | grep -q "\.$session_name"; then
                running="true"
            fi
            eval "${ServerInstances[$name]}"
            if ! $first; then json+=","; fi
            json+="\"$name\": {\"port\": \"$Port\", \"hostname\": \"$HostName\", \"map\": \"$StartMap\", \"maxplayers\": \"$MaxPlayers\", \"running\": $running}"
            first=false
        done
        json+="}"
        echo "$json"
        ;;

    get_plugins)
        available="["
        first=true
        for d in "$PluginSourceDir"/*/; do
            [ -d "$d" ] || continue
            pname=$(basename "$d")
            if [ ! -f "$ReceiptsDir/$pname.receipt" ]; then
                if ! $first; then available+=","; fi
                available+="\"$pname\""
                first=false
            fi
        done
        available+="]"

        installed="["
        first=true
        for f in "$ReceiptsDir"/*.receipt; do
            [ -f "$f" ] || continue
            pname=$(basename "$f" .receipt)
            if ! $first; then installed+=","; fi
            installed+="\"$pname\""
            first=false
        done
        installed+="]"
        
        echo "{\"available\": $available, \"installed\": $installed}"
        ;;
    
    deploy_server)
        Deploy-L4D2Server_NonInteractive
        ;;

    start_instance)
        Start-L4D2ServerInstance_NonInteractive "$1"
        ;;

    stop_instance)
        Stop-L4D2ServerInstance_NonInteractive "$1"
        ;;
    
    install_plugin)
        Invoke-PluginInstallation "$1"
        ;;

    uninstall_plugin)
        Invoke-PluginUninstallation "$1"
        ;;
        
    install_sourcemod)
        Install-SourceModAndMetaMod_NonInteractive
        ;;
    
    # --- �ļ�����ָ�� ---
    list_files)
        if ! Resolve-Path "$1"; then exit 1; fi

        if [ ! -d "$REAL_TARGET_PATH" ]; then
            echo "{\"success\": false, \"error\": \"Ŀ¼������: $REAL_TARGET_PATH\"}" >&2; exit 1
        fi
        
        json_parts=""
        shopt -s nullglob
        for f in "$REAL_TARGET_PATH"/*; do
            filename=$(basename "$f")
            filesize=$(du -sh "$f" | awk '{print $1}')
            filetype="file"
            if [ -d "$f" ]; then filetype="directory"; fi
            modtime=$(date -r "$f" +"%Y-%m-%d %H:%M:%S")

            file_json="{\"name\": $(json_escape "$filename"), \"size\": \"$filesize\", \"type\": \"$filetype\", \"mtime\": \"$modtime\"}"
            if [ -z "$json_parts" ]; then
                json_parts="$file_json"
            else
                json_parts="$json_parts,$file_json"
            fi
        done
        
        echo "{\"path\": $(json_escape "$1"), \"files\": [$json_parts], \"success\": true}"
        ;;

    get_file_content)
        if ! Resolve-Path "$1"; then exit 1; fi

        if [ ! -f "$REAL_TARGET_PATH" ]; then
            echo "����: �ļ�������" >&2; exit 1
        fi
        cat "$REAL_TARGET_PATH"
        ;;

    save_file_content)
        if ! Resolve-Path "$1"; then exit 1; fi
        
        mkdir -p "$(dirname "$REAL_TARGET_PATH")"
        
        if cat > "$REAL_TARGET_PATH"; then
            echo "�ļ� '$1' �ѱ��档"
        else
            echo "����: д���ļ� '$1' ʧ�ܡ�" >&2; exit 1
        fi
        ;;

    delete_path)
        if ! Resolve-Path "$1"; then exit 1; fi
        
        if [[ -z "$1" ]]; then echo "����: ·������Ϊ��" >&2; exit 1; fi
        if [ ! -e "$REAL_TARGET_PATH" ]; then
            echo "����: ·��������: '$REAL_TARGET_PATH'" >&2; exit 1
        fi
        
        if rm -rf "$REAL_TARGET_PATH"; then
            echo "·�� '$1' �ѱ�ɾ����"
        else
            echo "����: ɾ��·�� '$1' ʧ�ܡ�" >&2; exit 1
        fi
        ;;

    create_folder)
        if ! Resolve-Path "$1"; then exit 1; fi
        
        if mkdir -p "$REAL_TARGET_PATH"; then
            echo "�ļ��� '$1' �Ѵ�����"
        else
            echo "����: �����ļ��� '$1' ʧ�ܡ�" >&2; exit 1
        fi
        ;;
    
    unzip_file)
        if ! Resolve-Path "$1"; then exit 1; fi
        
        local target_dir=$(dirname "$REAL_TARGET_PATH")

        if [ ! -f "$REAL_TARGET_PATH" ]; then
            echo "����: �ļ�������: '$REAL_TARGET_PATH'" >&2; exit 1
        fi

        local output; local exit_code; local success=false

        case "$REAL_TARGET_PATH" in
            *.tar.gz|*.tgz)
            output=$(tar -xzf "$REAL_TARGET_PATH" -C "$target_dir" 2>&1)
            exit_code=$?
            ;;
            *.tar.bz2|*.tbz2)
            output=$(tar -xjf "$REAL_TARGET_PATH" -C "$target_dir" 2>&1)
            exit_code=$?
            ;;
            *.tar)
            output=$(tar -xf "$REAL_TARGET_PATH" -C "$target_dir" 2>&1)
            exit_code=$?
            ;;
            *.zip)
            output=$(unzip -o "$REAL_TARGET_PATH" -d "$target_dir" 2>&1)
            exit_code=$?
            ;;
            *.rar)
            output=$(unrar x -o+ "$REAL_TARGET_PATH" "$target_dir" 2>&1)
            exit_code=$?
            ;;
            *)
            echo "����: ��֧�ֵ�ѹ���ļ���ʽ��" >&2
            exit 1
            ;;
        esac

        if [ "$exit_code" -eq 0 ] || { [[ "$REAL_TARGET_PATH" == *.zip ]] && [ "$exit_code" -eq 1 ]; }; then
             success=true
        fi

        if [ "$success" = true ]; then
            echo "�ļ� '$1' �ѳɹ���ѹ��"
        else
            echo "��ѹ '$1' ʧ��: ${output:-δ֪����}" >&2; exit 1
        fi
        ;;
    *)
        echo "����: δ֪�� API Action: $ACTION" >&2
        exit 1
        ;;
esac