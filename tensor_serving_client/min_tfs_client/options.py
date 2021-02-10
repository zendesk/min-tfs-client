from typing import List, Tuple, Any

from abc import ABC

gRCPOptionsReturnType = List[Tuple[str, Any]]


class AbstractgRPCChannelOptions(ABC):
    """
    Abstract gRPC channel options abstract class

    This abstract class sets the interface that should be implemented in order to
    play nicely with gRPC channels option
    """
    def __repr__(self) -> gRCPOptionsReturnType:
        return [
            (f"grpc.{attr}", value)
            for attr, value in self.__dict__.items()
        ]


class gRPCChannelOptions(AbstractgRPCChannelOptions):
    """
    gRPC channel options DTO class.

    This class allows to pass additional gRPC options to manage channel.
    Right know it implements only two options:
        - max send message length
        - max receive message length

    But it could be easily extended in future as part of the functionality of current package,
    or implementer by user
    """

    def __init__(
            self,
            max_send_message_length: int = 512,
            max_receive_message_length: int = 512,
    ):
        """
        Object constructor
        :arg max_send_message_length: Maximum send message specified in Mb
        :arg max_send_message_length: Maximum send message specified in Mb
        """
        _mb_multiplier = 1024 * 1024
        self.max_receive_message_length = \
            max_receive_message_length * _mb_multiplier
        self.max_send_message_length = \
            max_send_message_length * _mb_multiplier


if __name__ == "__main__":
    a = gRPCChannelOptions()
    print(a.__repr__())
