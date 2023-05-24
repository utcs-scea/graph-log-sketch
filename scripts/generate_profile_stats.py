import sys
import os
import re

class BSP:
    def __init__(self, id: int) -> None:
        self.localities = {}
        self.id = id

    def readin(self, src_id: int, dst_id: int, pattern, type, counts: int, bytes: int):
        if src_id not in self.localities:
            self.localities[src_id] = Locality(src_id)
        self.localities[src_id].readin(dst_id, pattern, type, counts, bytes)

    def __str__(self):
        ret = f"BSP {self.id}\n"
        for locality_id in sorted(self.localities.keys()):
            ret += str(self.localities[locality_id])
        return ret

class Locality:
    def __init__(self, id: int) -> None:
        self.accesses = {}
        self.id = id

    def readin(self, dst_id: int, pattern, type, counts: int, bytes: int):
        if dst_id not in self.accesses:
            self.accesses[dst_id] = {}

        if pattern not in self.accesses[dst_id]:
            self.accesses[dst_id][pattern] = AccessPattern(pattern)

        self.accesses[dst_id][pattern].readin(type, counts, bytes)

    def __str__(self):
        ret = ""
        for dst_id in sorted(self.accesses.keys()):
            ret += f"# {self.id} {dst_id}\n"
            for pattern in sorted(self.accesses[dst_id].keys()):
                ret += str(self.accesses[dst_id][pattern])
        return ret

class AccessPattern:
    random = "RND"
    stream = "STR"

    def __init__(self, pattern) -> None:
        self.pattern = pattern
        self.access_type = {}

    def readin(self, type, counts: int, bytes: int):
        if type not in self.access_type:
            self.access_type[type] = AccessType(type)

        self.access_type[type].readin(counts, bytes)

    def __str__(self) -> str:
        ret = ""
        for type in sorted(self.access_type.keys()):
            ret += f"{self.pattern} {self.access_type[type]}\n"

        return ret

class AccessType:
    read = "RD"
    write = "WR"
    read_modify_write = "RMW"

    def __init__(self, type) -> None:
        self.type = type
        self.counts = 0
        self.bytes = 0

    def readin(self, counts: int, bytes: int):
        self.counts += counts
        self.bytes += bytes

    def __str__(self):
        return f"{self.type} {self.counts} {self.bytes}"


def main():
    stats = BSP(0)

    directory_name = sys.argv[1]
    data_file = ""
    try:
        data_file = sys.argv[2]
    except:
        pass

    for filename in os.listdir(directory_name):
        f = os.path.join(directory_name, filename)
        if os.path.isfile(f):
            with open(f, "r") as profile_file:
                lines = profile_file.readlines()
                for line in lines:
                    token = line.strip().split()

                    if len(token) == 0: 
                        continue

                    # get locality ID
                    src_id = int(re.search(r'\d+', token[1]).group(0))
                    
                    # preprocess '[id]'
                    token[2] = token[2].replace('[', ':')
                    token[2] = token[2].replace(']', '')
                    token[2] = token[2].replace('=', ':')
                    token[2] = token[2].replace('::', ':')

                    access_pattern = token[2].split(':')[1]
                    value = int(token[2].split(':')[-1])

                    if access_pattern == "remote_file_read_size":
                        if value != 0:
                            print(f"locality {src_id} read remote file {value} bytes")
                    elif access_pattern == "local_file_read_size":
                        if value != 0:
                            print(f"locality {src_id} read local file {value} bytes")
                    elif access_pattern == "local_seq_write_size":
                        stats.readin(src_id, src_id, AccessPattern.stream, AccessType.write, 0, value)
                    elif access_pattern == "local_rand_write_size":
                        stats.readin(src_id, src_id, AccessPattern.random, AccessType.write, 0, value)
                    elif access_pattern == "local_seq_read_size":
                        stats.readin(src_id, src_id, AccessPattern.stream, AccessType.read, 0, value)
                    elif access_pattern == "local_rand_read_size":
                        stats.readin(src_id, src_id, AccessPattern.random, AccessType.read, 0, value)
                    elif access_pattern == "local_seq_write_count":
                        stats.readin(src_id, src_id, AccessPattern.stream, AccessType.write, value, 0)
                    elif access_pattern == "local_rand_write_count":
                        stats.readin(src_id, src_id, AccessPattern.random, AccessType.write, value, 0)
                    elif access_pattern == "local_seq_read_count":
                        stats.readin(src_id, src_id, AccessPattern.stream, AccessType.read, value, 0)
                    elif access_pattern == "local_rand_read_count":
                        stats.readin(src_id, src_id, AccessPattern.random, AccessType.read, value, 0)
                    elif access_pattern == "remote_seq_read_size":
                        dst_id = int(token[2].split(':')[-2])
                        stats.readin(src_id, dst_id, AccessPattern.stream, AccessType.read, 0, value)
                    elif access_pattern == "remote_seq_read_count":
                        dst_id = int(token[2].split(':')[-2])
                        stats.readin(src_id, dst_id, AccessPattern.stream, AccessType.read, value, 0)
                    elif access_pattern == "remote_rand_read_size":
                        dst_id = int(token[2].split(':')[-2])
                        stats.readin(src_id, dst_id, AccessPattern.random, AccessType.read, 0, value)
                    elif access_pattern == "remote_rand_read_count":
                        dst_id = int(token[2].split(':')[-2])
                        stats.readin(src_id, dst_id, AccessPattern.random, AccessType.read, value, 0)
                    elif access_pattern == "remote_rand_rmw_size":
                        dst_id = int(token[2].split(':')[-2])
                        stats.readin(src_id, dst_id, AccessPattern.random, AccessType.read_modify_write, 0, value)
                    elif access_pattern == "remote_rand_rmw_count":
                        dst_id = int(token[2].split(':')[-2])
                        stats.readin(src_id, dst_id, AccessPattern.random, AccessType.read_modify_write, value, 0)

    # output stats file
    with open(f'GAL_WF1_{len(stats.localities)}_1a_{data_file}.stats', 'w') as output:
        output.write(str(stats))

if __name__ == "__main__":
    main()


