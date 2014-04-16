#ifndef IPCONFIG_PACKET_H
#define IPCONFIG_PACKET_H

struct iovec;

int packet_open(void);
void packet_close(void);
int packet_send(struct netdev *dev, struct iovec *iov, int iov_len);
void packet_discard(struct netdev *dev);
int packet_recv(struct netdev *dev, struct iovec *iov, int iov_len);

#endif /* IPCONFIG_PACKET_H */
