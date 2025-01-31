# Linux Development Environment for C/C++ and Python
FROM ubuntu:latest

# Working Directory
WORKDIR /

# Timezone Shit
ENV TZ=America/New_York
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

# Locale Shit
RUN apt-get update
RUN apt-get install -y locales locales-all
ENV LC_ALL en_US.UTF-8
ENV LANG en_US.UTF-8
ENV LANGUAGE en_US.UTF-8

# Install Essential Packages
RUN apt-get update && apt-get install -y \
    build-essential \
    ca-certificates \
    clang \
    clang-format \
    cmake \
    curl \
    electric-fence \
    gcc \
    gdb \
    git \
    llvm \
    login \
    passwd \
    make \
    nano \
    net-tools \
    netcat \
    nmap \
    npm \
    openssh-client \
    openssh-server \
    ssh \
    sudo \
    time \
    valgrind \
    vim \
    wget \
    xz-utils

# OpenSSL
RUN apt-get install -y \
    libssl-dev \
    openssl

# Open Ports for SSH, HTTP, HTTPS
EXPOSE 80 443 22

# Install Libraries
RUN apt-get install -y \
    libboost-all-dev \
    zlib1g-dev \
    libbz2-dev \
    libreadline-dev \
    libsqlite3-dev \
    libncurses5-dev \
    libncursesw5-dev \
    libffi-dev \
    liblzma-dev \
    libzmq3-dev

# Add Sudo User
RUN useradd -m \
    -d /home/celebrimbor \
    -s /bin/bash \
    -G sudo \
    celebrimbor && \
    echo "celebrimbor:celebrimbor" | chpasswd && \
    adduser celebrimbor sudo && \
    echo "celebrimbor ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers
    
# Switch to User
USER celebrimbor
WORKDIR /home/celebrimbor

# ARG GITHUB_TOKEN=${GITHUB_TOKEN}
# ARG GITHUB_USERNAME=${GITHUB_USER}
# ARG GITHUB_EMAIL=${GITHUB_EMAIL}

# # Clone Git Repo
# RUN cd /home/celebrimbor && \
#     clone https://${GITHUB_TOKEN}:git@github.com:ngrav007/http-proxy.git && \
#     cd http-proxy && \
#     git config --global user.name ${GITHUB_USERNAME} && \
#     git config --global user.email ${GITHUB_EMAIL}

# Update Bash Prompt
RUN echo "# Custom Bash Prompt" >> /home/celebrimbor/.bashrc && \
    echo "PS1='\[\e[0;1;38;5;226m\]\u \[\e[0;38;5;33m\]\W \[\e[0;38;5;246m\]\$ \[\e[0m\]>\[\e[0m\] '\n" >> /home/celebrimbor/.bashrc && \
    echo "# Aliases" >> /home/celebrimbor/.bashrc && \
    echo "alias ls='ls --color=auto'" >> /home/celebrimbor/.bashrc && \
    echo "alias la='ls -a'" >> /home/celebrimbor/.bashrc && \
    echo "alias ll='ls -l'" >> /home/celebrimbor/.bashrc && \
    echo "alias l='ls -la'" >> /home/celebrimbor/.bashrc && \
    echo "alias ..='cd ..'\n" >> /home/celebrimbor/.bashrc
