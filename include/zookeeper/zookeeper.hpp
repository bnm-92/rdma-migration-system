/**
 * 
 * ZooKeeper C++ Wrapper API
 * This library only supports features needed for RaMP and is tested to meet its specifications
 * User discretion advised
 * The API style, comments and method definition are copied from project mesos however underlying implementation 
 * only uses zookeeper c client API
 * 
 */

#ifndef RAMP_ZOOKEEPER
#define RAMP_ZOOKEEPER

#include <utils/miscutils.hpp>
#include <atomic>
#include <zookeeper/zookeeper.h>


static int BUFLEN = 4096;

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

  /**
   * \brief get the state of the zookeeper connection.
   *
   * The return value will be one of the \ref State Consts.
   */
  int getState();

  /**
   * \brief get the current session id.
   *
   * The current session id or 0 if no session is established.
   */
  int64_t getSessionId();

  /**
   * \brief get the current session timeout.
   *
   * The session timeout requested by the client or the negotiated
   * session timeout after the session is established with
   * ZooKeeper. Note that this might differ from the initial
   * `sessionTimeout` specified when this instance was constructed.
   */
  int64_t getSessionTimeout() const;

  /**
   * \brief create a node synchronously.
   *
   * Please note that this client does not support recursive node creation
   * 
   * This method will create a node in ZooKeeper. A node can only be
   * created if it does not already exists. The Create Flags affect
   * the creation of nodes.  If ZOO_EPHEMERAL flag is set, the node
   * will automatically get removed if the client session goes
   * away. If the ZOO_SEQUENCE flag is set, a unique monotonically
   * increasing sequence number is appended to the path name.
   *
   * \param path The name of the node. Expressed as a file name with
   *    slashes separating ancestors of the node.
   * \param data The data to be stored in the node.
   * \param acl The initial ACL of the node. If null, the ACL of the
   *    parent will be used.
   * \param flags this parameter can be set to 0 for normal create or
   *    an OR of the Create Flags
   * \param result A string which will be filled with the path of
   *    the new node (this might be different than the supplied path
   *    because of the ZOO_SEQUENCE flag).  The path string will always
   *    be null-terminated.
   * \param recursive if true, attempts to create all intermediate
   *   znodes as required; note that 'flags' and 'data' will only be
   *   applied to the creation of 'basename(path)'.
   * \return  one of the following codes are returned:
   * ZOK operation completed successfully
   * ZNONODE the parent node does not exist.
   * ZNODEEXISTS the node already exists
   * ZNOAUTH the client does not have permission.
   * ZNOCHILDRENFOREPHEMERALS cannot create children of ephemeral nodes.
   * ZBADARGUMENTS - invalid input parameters
   * ZINVALIDSTATE - state is ZOO_SESSION_EXPIRED_STATE or ZOO_AUTH_FAILED_STATE
   * ZMARSHALLINGERROR - failed to marshall a request; possibly, out of memory
   */
  int create(
      const std::string& path,
      const std::string& data,
      const ACL_vector& acl,
      int flags,
      std::string* result);

  /**
   * \brief delete a node in zookeeper synchronously.
   *
   * \param path the name of the node. Expressed as a file name with
   *    slashes separating ancestors of the node.
   * \param version the expected version of the node. The function
   *    will fail if the actual version of the node does not match the
   *    expected version. If -1 is used the version check will not take
   *    place.
   * \return one of the following values is returned.
   * ZOK operation completed successfully
   * ZNONODE the node does not exist.
   * ZNOAUTH the client does not have permission.
   * ZBADVERSION expected version does not match actual version.
   * ZNOTEMPTY children are present; node cannot be deleted.
   * ZBADARGUMENTS - invalid input parameters
   * ZINVALIDSTATE - state is ZOO_SESSION_EXPIRED_STATE or ZOO_AUTH_FAILED_STATE
   * ZMARSHALLINGERROR - failed to marshall a request; possibly, out of memory
   */
  int remove(const std::string& path, int version);

  /**
   * \brief checks the existence of a node in zookeeper synchronously.
   *
   * \param path the name of the node. Expressed as a file name with
   *    slashes separating ancestors of the node.
   * \param watch has been removed, can only be set when a callback is specified, please use wget
   * \param stat the return stat value of the node.
   * \return return code of the function call.
   * ZOK operation completed successfully
   * ZNONODE the node does not exist.
   * ZNOAUTH the client does not have permission.
   * ZBADARGUMENTS - invalid input parameters
   * ZINVALIDSTATE - state is ZOO_SESSION_EXPIRED_STATE or ZOO_AUTH_FAILED_STATE
   * ZMARSHALLINGERROR - failed to marshall a request; possibly, out of memory
   */
  int exists(const std::string& path, Stat* stat);

  int wexists(const std::string& path, Stat* stat, watcher_fn watcher, void* watcherCtx);

  /**
   * \brief gets the data associated with a node synchronously.
   *
   * \param path the name of the node. Expressed as a file name with
   *    slashes separating ancestors of the node.
   * \param watch has been removed, can only be set when a callback is specified, please use wget
   * \param result the data returned by the server
   * \param stat if not `nullptr`, will hold the value of stat for the path
   *    on return.
   * \return return value of the function call.
   * ZOK operation completed successfully
   * ZNONODE the node does not exist.
   * ZNOAUTH the client does not have permission.
   * ZBADARGUMENTS - invalid input parameters
   * ZINVALIDSTATE - state is ZOO_SESSION_EXPIRED_STATE or ZOO_AUTH_FAILED_STATE
   * ZMARSHALLINGERROR - failed to marshall a request; possibly, out of memory
   */
  int get(
      const std::string& path,
      std::string* result,
      Stat* stat);

  int wget(
      const std::string& path,
      std::string* result,
      Stat* stat, 
      watcher_fn watcher, 
      void* watcherCtx);

  /**
   * \brief lists the children of a node synchronously.
   *
   * \param path the name of the node. Expressed as a file name with
   *   slashes separating ancestors of the node.
   * \param watch has been removed, can only be set when a callback is specified, please use wgetChildred
   * \param results return value of children paths.
   * \return the return code of the function.
   * ZOK operation completed successfully
   * ZNONODE the node does not exist.
   * ZNOAUTH the client does not have permission.
   * ZBADARGUMENTS - invalid input parameters
   * ZINVALIDSTATE - state is ZOO_SESSION_EXPIRED_STATE or ZOO_AUTH_FAILED_STATE
   * ZMARSHALLINGERROR - failed to marshall a request; possibly, out of memory
   */
  int getChildren(
      const std::string& path,
      std::vector<std::string>* results);

  int wgetChildren(
      const std::string& path,
      std::vector<std::string>* results,
      watcher_fn watcher, 
      void* watcherCtx);

  /**
   * \brief sets the data associated with a node.
   *
   * \param path the name of the node. Expressed as a file name with slashes
   * separating ancestors of the node.
   * \param data the data to be written to the node.
   * \param version the expected version of the node. The function will fail if
   * the actual version of the node does not match the expected version. If -1 is
   * used the version check will not take place.
   * \return the return code for the function call.
   * ZOK operation completed successfully
   * ZNONODE the node does not exist.
   * ZNOAUTH the client does not have permission.
   * ZBADVERSION expected version does not match actual version.
   * ZBADARGUMENTS - invalid input parameters
   * ZINVALIDSTATE - zhandle state is either ZOO_SESSION_EXPIRED_STATE or ZOO_AUTH_FAILED_STATE
   * ZMARSHALLINGERROR - failed to marshall a request; possibly, out of memory
   */
  int set(const std::string& path, const std::string& data, int version);

  /**
   * \brief return a message describing the return code.
   *
   * \return string message corresponding to return code.
   */
  std::string message(int code) const;

    /**
   * \brief returns whether or not the specified return code implies
   * the operation can be retried "as is" (i.e., without needing to
   * change anything).
   *
   * \return bool indicating whether operation can be retried.
   */
  bool retryable(int code);

    /**
     * Converts return codes to readable values, simple conversion from enum to std::string
     * 
    */
  std::string toString(int code);

  /**
   * Set Debug level to the file specified/stderr
   * Levels are:
   * ZOO_LOG_LEVEL_ERROR
   * ZOO_LOG_LEVEL_WARN
   * ZOO_LOG_LEVEL_INFO 
   * ZOO_LOG_LEVEL_DEBUG
  */
  void SetDebugLevel(ZooLogLevel logLevel);


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

/**
 * global watcher callbacks, passed the this parameter as context, this callback will be executed whenever
 * a watch is set without a function definition, please note, this API prevents this by removing the watch bool
 * parameter and sets it as false by default
 * 
 * Please use wget, wexist and other w operations to set watches and set callbacks and context as the following method
    typedef void (*watcher_fn)(zhandle_t *zh, int type, int state, const char *path,void *watcherCtx);
    simply use a lambda as 
    [](zhandle_t *zh, int type, int state, const char *path,void *watcherCtx) {
        your code
    }
*/ 
void global_watch_CB (zhandle_t *zzh, int type, int state, const char *path, void* context) {
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
