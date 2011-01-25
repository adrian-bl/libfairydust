#!/usr/bin/perl
use strict;
use Getopt::Long;

#
# Simple allocator daemon for  use with libfairydust
#
# (C) 2010 Adrian Ulrich
#
# Version 0.98  - Initial release
# Version 1.00  - Use syslog for $self->xlog output
# Version 1.01  - Add Getopts and --daemon option


my $VERSION = 1.01;
my $getopts = {};
GetOptions($getopts, "help|h", "version|v", "daemon|d") or exit 1;

if($getopts->{help}) {
	die << "EOF"
Usage: $0 [--help --version --daemon]

  -h, --help      print this help and exit.
  -v, --version   print version and exit.
  -d, --daemon    fork into background after startup.

Mail bug reports and flames to <adrian\@blinkenlights.ch>
EOF
}
elsif($getopts->{version}) {
	die "fairyd.pl version $VERSION\n";
}
else {
	my $nvr = Adrian::Fairyd->new;
	$nvr->init(Port=>6680);
	$nvr->daemonize if $getopts->{daemon};
	$nvr->run;
}




#############################################################################################

package Adrian::Fairyd;

	use strict;
	use Fcntl ':flock';
	use Sys::Syslog qw(:standard :macros);
	use Carp;
	use POSIX qw(setsid ceil);
	use IO::Socket;
	use List::Util qw(shuffle);
	use Data::Dumper;
	
	#############################################################
	# Creates a new Fairyd object
	sub new {
		my($classname, %args) = @_;
		my $self = { free=>{}, used=>{}, sockets=>{}, dsock=>undef, hostname=>undef };
		
		return bless($self,$classname);
	}
	
	#############################################################
	# 'Allocate' free nvidia devices
	sub init {
		my($self, %args) = @_;
		
		openlog("fairyd", "ndelay,pid", "local");
		
		my $nvport = delete($args{Port}) or $self->panic("No Port argument!");
		$self->xlog("Scanning and locking nvidia devices and bind()'ing to port $nvport");
		for(my $i=0; $i<=0xFF; $i++) {
			my $nvdev = "/dev/nvidia$i";
			last unless -e $nvdev; # hit end
			
			my $hr = { fh=>undef, id=>$i, devname=>$nvdev };
			open($hr->{fh}, $nvdev) or next;
			
			if(flock($hr->{fh}, LOCK_EX|LOCK_NB)) {
				$self->xlog("+ managing $nvdev");
				$self->register_device($hr);
			}
			else {
				$self->xlog("- skipping $nvdev ($!)");
				close($hr->{fh});
			}
		}
		
		my $sock = IO::Socket::INET->new(LocalPort=>$nvport, LocalAddr=>'127.0.0.1', Listen=>256, Proto=>'tcp', ReuseAddr=>1) 
		  or $self->panic("Could not listen on ".$nvport." : $!");
		
		$self->{hostname} = `hostname -s`; chomp($self->{hostname});
		$self->xlog("My own hostname seems to be `$self->{hostname}'");
		
		$self->{dsock} = Adrian::Fairyd::Danga->new(sock=>$sock, on_read_ready => sub { $self->_dx_accept(shift) });
	}
	
	#############################################################
	# Enter Danga Eventloop
	sub run {
		my($self) = @_;
		$self->xlog("fairyd is ready: entering event loop");
		$self->free_devs(1);
		Danga::Socket->EventLoop();
	
	}
	
	#############################################################
	# Fork into background
	sub daemonize {
		my($self) = @_;
		print "$0: forking into background\n";
		chdir("/")               or $self->panic("Could not change to / : $!");
		umask(0);
		defined(my $pid = fork()) or $self->panic("fork() failed: $!");   # error
		exit(0) if $pid;                                                  # master
		setsid();
	}
	
	#############################################################
	# Danga Callback for new connections
	sub _dx_accept {
		my($self,$dsock) = @_;
		my $new_sock  = $dsock->sock->accept;
		my $ndsock    = $self->{sockets}->{$new_sock} = { dsock=>Adrian::Fairyd::Danga->new(sock=>$new_sock, on_read_ready => sub { $self->_dx_read(shift) }), buff=>'', bsize=>0 };
		$self->xlog("<$new_sock> - accepting new connection from ".$self->{sockets}->{$new_sock}->{dsock}->peer_addr_string);
	}
	
	#############################################################
	# Danga Callback for received data
	sub _dx_read {
		my($self,$dsock) =  @_;
		
		my $bref = $dsock->read(1024);
		my $sref = $self->{sockets}->{$dsock->sock} or $self->panic("Socket is not registered!");
		
		if(defined($bref) && $sref->{bsize} <= 1024*8) {
			# new data AND our buffer is smaller than 8kb? -> accept it!
			$sref->{buff}  .= $$bref;
			$sref->{bsize} += length($$bref);
			
			if($sref->{bsize} >= 2 && substr($sref->{buff},-2,2) eq "\r\n") {
				# -> received a complete command (we do not support pipelining)
				my $cmd = substr($sref->{buff},0,-2);  # ditch \r\n
				$sref->{buff} = "";
				$sref->{bsize}= 0;
				
				if($cmd =~ /^allocate (\d+)/) {
					my @devlist  = ();
					my $launcher = undef;
					my $owner    = $self->get_uid_of_pid($1);
					
					$self->xlog("PID $1 (UID=$owner) would like to allocate nvidia devices");
					
					if($self->{hostname} =~ /^brutus/) {
						$self->xlog("Running on login node -> sending ALL devices to client");
						@devlist = keys(%{$self->{free}});
					}
					else {
						$launcher = $self->find_launcher_of($1);
						if(defined($launcher)) {
							@devlist = $self->allocate_devs_for($launcher);
							$self->xlog("launcher of $1 is $launcher, av-devs=@devlist");
						}
						else {
							$self->xlog("Failed to find launcher of $1");
						}
					}
					$self->send_devices($dsock,\@devlist);
				}
				elsif($cmd eq 'debug') {
					$dsock->write(Data::Dumper::Dumper($self));
				}
				else {
					$dsock->write("- unknown command\r\n");
				}
				
			}
			
		}
		else {
			# -> close
			return $self->_dx_close($dsock);
		}
		
	}
	
	#############################################################
	# deregister socket (called by read callback)
	sub _dx_close {
		my($self,$dsock) = @_;
		
		my $real_sock = $dsock->sock;
		$self->xlog("<$real_sock> - peer disconnected");
		delete($self->{sockets}->{$real_sock}) or $self->panic("$real_sock was not registered!");
		$dsock->close;
	}
	
	#############################################################
	# Send device id's to remote
	sub send_devices {
		my($self,$ds,$devlist) = @_;
		$ds->write(join(" ",@$devlist)."\r\n");
	}
	
	
	#############################################################
	# add new item to freelist
	sub register_device {
		my($self, $what) = @_;
		
		my $id = $what->{id};
		$self->panic("No id!")                 unless defined $id;
		$self->panic("Duplicate id $id!")      if exists($self->{free}->{$id});
		$self->panic("id $id marked as used!") if exists($self->{used}->{$id});
		$self->{free}->{$id} = $what;
	}
	
	
	#############################################################
	# Remove device from registration (Can only deregister if free)
	sub deregister_device {
		my($self,$id) = @_;
		delete($self->{free}->{$id}) or $self->panic("ID $id was not in $self->{free} !");
	}
	
	#############################################################
	# Walk 'upwards' until we find the launcher of given PID
	sub find_launcher_of {
		my($self,$pid) = @_;
		
		$self->xlog("Searching launcher of pid $pid");
		
		while($pid) {
			
			my $this_name = undef;
			my $this_ppid = undef;
			
			open(PROCFS_STATUS, "<", "/proc/$pid/status") or return undef;
			
			while(<PROCFS_STATUS>) {
				if($_ =~ /^Name:\s+(.+)$/)  { $this_name = $1 }
				if($_ =~ /^PPid:\s+(\d+)$/) { $this_ppid = $1 }
			}
			close(PROFCS_STATUS);
			
			if(defined($this_name) && defined($this_ppid)) {
				$self->xlog("ParentPid of ($this_name) $pid is $this_ppid");
				
				if($this_name eq 'res' or $this_name eq 'xterm') { # xterm -> debug on echelon
					return $pid;
				}
				else {
					$pid = $this_ppid;
				}
			}
		}
		return undef; # failed to find launcher (for whatever reason)
	}
	
	#############################################################
	# Returns the UID of given pid
	sub get_uid_of_pid {
		my($self,$pid) = @_;
		my @stat = stat("/proc/$pid");
		return $stat[4];
	}
	
	#############################################################
	# Check if the registered pids are still there and move
	# free ressources back into $self->{unused}
	sub free_devs {
		my($self, $add_timer) = @_;
		
		foreach my $pid (keys(%{$self->{used}})) {
			next if -d "/proc/$pid"; # process running
			# else: move everything out of this struct
			my $vanished = delete($self->{used}->{$pid});
			
			$self->xlog("freed ressources of pid $pid");
			foreach my $xref (values(%$vanished)) {
				$self->{free}->{$xref->{id}} = $xref;
			}
			
		}
		
		if($add_timer) {
			Danga::Socket->AddTimer(15, sub{ $self->free_devs(1) } );
		}
		
	}
	
	#############################################################
	# Try to grab some devices for $lpid
	sub allocate_devs_for {
		my($self,$lpid) = @_;
		
		if(!exists($self->{used}->{$lpid})) {
			# trigger a cleanup
			$self->free_devs(0);
			
			my $given_devs   = {};
			my $gpu_per_core = 0;
			my @lsb_hosts    = ();
			my $rcounter     = {};
			my $local_gdevs  = 0;
			my $xenv         = $self->get_environment_of($lpid);
			
			if(defined($xenv)) {
				$gpu_per_core = (abs($xenv->{FDUST_GPU_PER_CORE}||0));
				$xenv->{LSB_HOSTS} =~ tr/a-zA-Z0-9 //cd; # ??
				@lsb_hosts    = split(/ /,$xenv->{LSB_HOSTS});
			}
			
			$self->xlog("$lpid runs on `@lsb_hosts' and requests $gpu_per_core GPU(s) per core");
			
			foreach my $hostname (@lsb_hosts) {
				$rcounter->{$hostname} += $gpu_per_core;
			}
			
			$local_gdevs = ceil($rcounter->{$self->{hostname}});
			
			$self->xlog("$lpid shall have $local_gdevs GPU(s) on this host.");
			
			# Copy of all free devices
			my @free_list = $self->get_sorted_freedevs;
			
			$self->xlog("Smart list: ".join(",",@free_list));
			$self->xlog("RandomList: ".join(",",keys(%{$self->{free}})));
			
			foreach my $devid (@free_list) {
				last if int(keys(%$given_devs)) == $local_gdevs;
				$given_devs->{$devid} = delete($self->{free}->{$devid});
			}
			$self->{used}->{$lpid} = $given_devs;
		}
		return keys(%{$self->{used}->{$lpid}});
	}
	
	
	#############################################################
	# Return a 'smartly' sorted list of all (free) GPU devices
	sub get_sorted_freedevs {
		my($self) = @_;
		
		my $gpus_per_bus = 2;
		my @free_list    = keys(%{$self->{free}});
		my $buckets      = {};
		my $bprio        = {};
		my @result       = ();
		
		# Order devices by pci-slot
		map{ push(@{$buckets->{int($_/$gpus_per_bus)}}, $_) } @free_list;
		
		# re-order by 'priority' (high = much free bandwidth)
		map{ push(@{$bprio->{int(@{$buckets->{$_}})}}, delete($buckets->{$_})) } keys(%$buckets);
		
		
		REDO:
		foreach my $bsize (sort({$b<=>$a} keys(%$bprio))) {
			my $this = delete($bprio->{$bsize});
			for(my $i=0; $i<$bsize; $i++) {
				foreach my $pair (shuffle(@$this)) {
					push(@result,pop(@$pair));
				}
				push(@{$bprio->{$bsize-1}}, @$this);
				goto REDO;
			}
		}
		
		return @result;
	}
	
	
	sub get_environment_of {
		my($self,$pid) = @_;
		my $rv = undef;
		
		{
			local $/ = chr(0);
			open(ENV, "<", "/proc/$pid/environ") or return $rv;
			$rv = {};
			while(<ENV>) {
				if($_ =~ /^([^=]+)=(.*)$/) {
					$rv->{$1} = $2;
				}
			}
			close(ENV);
		}
		
		return $rv;
	}
	
	sub panic {
		my($self, @msg) = @_;
		$self->xlog('PANIC: ',@msg);
		print join(" ", 'PANIC:', @msg)."\n";
		print "--- backtrace ---\n";
		print Carp::confess();
		die;die;die;
	}
	
	sub xlog {
		my($self, @msg) = @_;
		syslog(LOG_INFO,join(" ",@msg));
	}
	
1;


###############################################################################################################
# Danga-Socket event dispatcher
package Adrian::Fairyd::Danga;
	use strict;
	use base qw(Danga::Socket);
	use fields qw(on_read_ready on_error on_hup);
	
	sub new {
		my($self,%args) = @_;
		$self = fields::new($self) unless ref $self;
		$self->SUPER::new($args{sock});
		
		foreach my $field qw(on_read_ready on_error on_hup) {
			$self->{$field} = $args{$field} if $args{$field};
		}
		
		$self->watch_read(1) if $args{on_read_ready}; # Watch out for read events
		return $self;
	}
	
	sub event_read {
		my($self) = @_;
		if(my $cx = $self->{on_read_ready}) {
			return $cx->($self);
		}
	}
	
	sub event_err {
		my($self) = @_;
		if(my $cx = $self->{on_error}) {
			return $cx->($self);
		}
	}
	
	sub event_hup {
		my($self) = @_;
		if(my $cx = $self->{on_hup}) {
			return $cx->($self);
		}
	}
	
1;

