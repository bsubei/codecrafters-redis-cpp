import socket

# Replace with your Redis server details
redis_host = 'localhost'
redis_port = 6379

# Create a socket object
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

# Connect to the Redis server
sock.connect((redis_host, redis_port))

# Prepare the raw command
raw_command = b"+PING\r\n"

# Send the raw command
sock.sendall(raw_command)

# Receive the response
response = sock.recv(1024)

# Print the response
print(response.decode())  # Should print "+PONG\r\n"

# Close the socket
sock.close()

