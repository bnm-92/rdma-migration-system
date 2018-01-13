/**
 * 
 * ZooKeeper C++ Wrapper API
 * This library only supports features needed for RaMP and is tested to meet its specifications
 * User discretion advised
 * 
 */

#ifndef RAMP_ZOOKEEPER
#define RAMP_ZOOKEEPER

#include <utils/miscutils.hpp>
#include <atomic>
#include <zookeeper/zookeeper.h>

class ZooKeeper {
public:
  /**
   * \brief instantiate new ZooKeeper client.
   *
   * The constructor initiates a new session, the session is established 
   * synchronously
   * 
   * \param servers comma-separated host:port pairs, each corresponding
   *    to a ZooKeeper server. e.g. "127.0.0.1:3000,127.0.0.1:3001"
   * \param int value in milliseconds
   * \param logfile for outputting errors/warning, default setting is
   *  Warnings and errors
   */
    ZooKeeper(const std::string& servers,
            const int64_t& sessionTimeout,
            FILE * logFile);

    ~ZooKeeper();

    void Close();

    /**
     * Used synchronousy connect to the server
    */
    std::atomic<int> connected;
private:
    /* ZooKeeper instances are not copyable. */
    ZooKeeper(const ZooKeeper& that);
    ZooKeeper& operator=(const ZooKeeper& that);

    /**
     * \brief ZooKeeper handle.
     * 
     * This is the handle that represents a connection to the ZooKeeper service.
     * It is needed to invoke any ZooKeeper function. A handle is obtained using
     * \ref zookeeper_init.
     */
    zhandle_t *zh;
    
    /**
     * \brief client id structure.
     * 
     * This structure holds the id and password for the session. This structure
     * should be treated as opaque. It is received from the server when a session
     * is established and needs to be sent back as-is when reconnecting a session.
     */
    clientid_t z_cid;     

    /**
     * Host port, used to connect to zookeeper servers
     */
    int hostport = 8555;

    /**
     * Session Time out in milliseconds
    */
    int64_t sessionTimeout;

    /**
     * list of zookeeper server host names and ports
    */
    std::string list_zservers;

    /**
     * 
    */
};

// global watcher callbacks, passed the this parameter as context
void synch_connection_CB (zhandle_t *zzh, int type, int state, const char *path, void* context) {
    ZooKeeper *zk = (ZooKeeper*)context;
    if (type == ZOO_SESSION_EVENT) {
        if (state == ZOO_CONNECTED_STATE) {
            zk->connected++;
        } else if (state == ZOO_AUTH_FAILED_STATE) {
            zookeeper_close(zzh);                                  
            LogError("Zookeeper Auth Failed");
        } else if (state == ZOO_EXPIRED_SESSION_STATE) {           
            zookeeper_close(zzh);
            LogError("Session State expired for Zookeeper");
        }
    }
}

#include<zookeeper/zookeeper.tpp>

#endif // RAMP_ZOOKEEPER
