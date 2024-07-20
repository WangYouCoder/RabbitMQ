#include "../mqcommon/logger.hpp"
#include "../mqcommon/util.hpp"

int main()
{
    UtilFile uf("../mqcommon/logger.hpp");
    // DLOG("文件是否存在: %d", uf.exists());
    // DLOG("文件大小: %ld", uf.size());

    UtilFile t("./aaa/bb/cc/tmp.cc");
    if(t.exists() == false)
    {
        std::string path = UtilFile::parentDirectory("./aaa/bb/cc/tmp.cc");
        if(UtilFile(path).exists() == false)
        {
            UtilFile::createDirectory(path);
        }
        UtilFile::createFile("./aaa/bb/cc/tmp.cc");
    }

    std::string body;
    uf.read(body);
    t.write(body);

    UtilFile::removeDirectory("./aaa");
    return 0;
}