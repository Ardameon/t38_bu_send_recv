# t38_bu_send_recv
Application for testing fax_bu_app. It sends/receives T.38-fax through fax_bu_app gateway.

recv ------------------  send
--->| t38_bu_send_recv | ---
|    ------------------    |
|                          |
|                          |
|    ------------------    |
----|    fax_bu_app    |<---
     ------------------ 
