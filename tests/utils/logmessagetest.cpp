#include "../../include/utils/miscutils.hpp"

int main() {
    LogMessage(4, "MESSAGE", "this is a message");
    LogError("this is an error message");
    LogWarning("this is a warning message");
    LogInfo("this is an info message");

    LogAssert(1==1, "1==1");
    LogAssert(1!=1, "1!=1");


    LogError("Could not find expected server information for server %ld", 2l);

    return 0;
}
