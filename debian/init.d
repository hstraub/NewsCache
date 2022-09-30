#! /bin/sh
### BEGIN INIT INFO
# Provides:          newscache
# Required-Start:    $remote_fs $syslog $named
# Required-Stop:     $remote_fs $syslog
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Description:       NNTP caching daemon
### END INIT INFO

PATH=/usr/local/sbin:/usr/local/bin:/sbin:/bin:/usr/sbin:/usr/bin
DAEMON=/usr/sbin/newscache
NAME=newscache
DESC=newscache

test -x $DAEMON || exit 0
test -f /etc/newscache.conf || exit 0

# Include newscache defaults if available
if [ -f /etc/default/newscache ] ; then
	. /etc/default/newscache
fi

set -e

case "$1" in
  start)
	echo -n "Starting $DESC: "
	start-stop-daemon --start --quiet --pidfile /var/run/$NAME.pid \
		--exec $DAEMON -- $DAEMON_OPTS
	echo "$NAME."
	;;
  stop)
	echo -n "Stopping $DESC: "
	start-stop-daemon --stop --oknodo --quiet --pidfile /var/run/$NAME.pid \
		--exec $DAEMON
	echo "$NAME."
	;;
  reload)
	echo "Reloading $DESC configuration files."
	start-stop-daemon --stop --signal 1 --quiet --pidfile \
		/var/run/$NAME.pid --exec $DAEMON
  ;;
  restart|force-reload)
	#
	#	If the "reload" option is implemented, move the "force-reload"
	#	option to the "reload" entry above. If not, "force-reload" is
	#	just the same as "restart".
	#
	echo -n "Restarting $DESC: "
	$0 stop
	sleep 2
	$0 start
	echo "$NAME."
	;;
  *)
	N=/etc/init.d/$NAME
	# echo "Usage: $N {start|stop|restart|reload|force-reload}" >&2
	echo "Usage: $N {start|stop|restart|force-reload}" >&2
	exit 1
	;;
esac

exit 0
