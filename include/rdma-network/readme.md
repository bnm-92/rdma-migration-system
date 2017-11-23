
This library is currently a heavily refactored and redesigned C++ port of this tutorial:
http://www.hpcadvisorycouncil.com/pdf/building-an-rdma-capable-application-with-ib-verbs.pdf
http://www.hpcadvisorycouncil.com/pdf/rdma-read-and-write-with-ib-verbs.pdf

It is meant to make usage of RDMA operations easy and intuitive by wrapping away the event-driven nature of RDMA into a more object-oriented interface. It does NOT maximize performance (i.e. does not incorporate techniques like unsignalled reads, as well as other techniques described in papers like FaRM and HERD).

To begin, we suggest you:
-   look at `server` and `client`, which are example test apps
-   look at the public interface in `rdma_server_prototype`, followed by the public interface in `rdma_server` and `rdma_client`

That should be more than enough for you to get started with using this library.

For more details on the underlying rdma libraries, see the above tutorials. Detailed manual pages for most RDMA methods are available at rdmamojo.com.

TODOs:
-   Handle shutdown properly and make sure both the connection event thread and work completion thread are spun down.
-   Fix memory leaks (this is dependent on stopping threads properly, so do that first). Namely, make sure device_resources gets deleted (and really just go double-check that every field gets cleaned up somewhere)
-   Replace asserts with exceptions
