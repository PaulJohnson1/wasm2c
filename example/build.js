const { spawnSync: spawn } = require("child_process")
const { readdirSync: readdir, lstatSync: lstat} = require("fs")

const files = readdir("./")

files.forEach((directory) =>
{
    if (!lstat(directory).isDirectory())
        return
    
    const files = readdir(directory)

    files.forEach(file =>
        spawn("../build/wasm2c", ["-i", directory + "/" + file, "-o", directory + "/" + file + ".c"])
    )
})
