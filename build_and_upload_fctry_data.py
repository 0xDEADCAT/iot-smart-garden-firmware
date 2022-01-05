import os
import subprocess

Import("env")

factory_data_csv = os.path.join(env.subst(env['PROJECT_DIR']), env.GetProjectOption("custom_factory_data"))
factory_data_bin = os.path.join(env.subst(env['BUILD_DIR']), os.path.splitext(os.path.basename(factory_data_csv))[0] + ".bin")

# Build factory data from CSV and upload it to the device
def upload_fctry_img(*args, **kwargs):
    platform = env.PioPlatform()
    python_exe = env['PYTHONEXE']
    esp_idf_path = platform.get_package_dir('framework-espidf')
    nvs_partition_generator = os.path.join(esp_idf_path, 'components', 'nvs_flash', 'nvs_partition_generator', 'nvs_partition_gen.py')
    parttool = os.path.join(esp_idf_path, 'components', 'partition_table', 'parttool.py')
    esptool = os.path.join(platform.get_package_dir('tool-esptoolpy'), "esptool.py")

    os.environ['IDF_PATH'] = esp_idf_path
    print(f"--- Generate {os.path.basename(factory_data_bin)} from CSV file ---")
    subprocess.run([python_exe, nvs_partition_generator, "generate", factory_data_csv, factory_data_bin, "0x3000"])
    print("--- Get fctry partition offset and size from device ---")
    parttool_output = subprocess.check_output([python_exe, parttool, "--port", env['UPLOAD_PORT'], "get_partition_info", "--partition-name=fctry"])
    parttool_output = parttool_output.decode("utf-8")
    offset, size = parttool_output.splitlines()[-1].split()
    print(f"--- Upload {os.path.basename(factory_data_bin)} to device ---")
    subprocess.run([python_exe, esptool, "--port", env['UPLOAD_PORT'], "write_flash", offset, factory_data_bin])

env.AddCustomTarget(
    name="upload_fctry_data",
    dependencies=factory_data_csv,
    actions=upload_fctry_img,
    title="Upload Factory Data",
    description="Upload Factory Data to the device"
)