script_folder="/home/groby/dev/isched"
echo "echo Restoring environment" > "$script_folder/deactivate_conanbuildenv-debug-x86_64.sh"
for v in PATH
do
    is_defined="true"
    value=$(printenv $v) || is_defined="" || true
    if [ -n "$value" ] || [ -n "$is_defined" ]
    then
        echo export "$v='$value'" >> "$script_folder/deactivate_conanbuildenv-debug-x86_64.sh"
    else
        echo unset $v >> "$script_folder/deactivate_conanbuildenv-debug-x86_64.sh"
    fi
done


export PATH="/home/groby/.conan2/p/ninja30e87f1e92502/p/bin:/home/groby/.conan2/p/cmake5a09891eb5f57/p/bin:$PATH"