import os
from pathlib import Path

import tensorflow as tf

tf.compat.v1.disable_eager_execution()


SCRIPT_PATH = Path(os.path.dirname(os.path.realpath(__file__)))


def main():
    session = tf.compat.v1.InteractiveSession()
    string_placeholder = tf.compat.v1.placeholder(tf.string, name="string_placeholder")
    float_placeholder = tf.compat.v1.placeholder(tf.float32, name="float32_placeholder")
    int_placeholder = tf.compat.v1.placeholder(tf.int64, name="int64_placeholder")

    string_predict = tf.identity(string_placeholder, name="string_predict")
    float_predict = tf.identity(float_placeholder, name="float32_predict")
    int_predict = tf.identity(int_placeholder, name="int64_predict")

    export_path = str(SCRIPT_PATH / "00000001")
    builder = tf.compat.v1.saved_model.builder.SavedModelBuilder(export_path)

    string_input = tf.compat.v1.saved_model.build_tensor_info(string_placeholder)
    float_input = tf.compat.v1.saved_model.build_tensor_info(float_placeholder)
    int_input = tf.compat.v1.saved_model.build_tensor_info(int_placeholder)

    string_output = tf.compat.v1.saved_model.build_tensor_info(string_predict)
    float_output = tf.compat.v1.saved_model.build_tensor_info(float_predict)
    int_output = tf.compat.v1.saved_model.build_tensor_info(int_predict)

    prediction_signature = tf.compat.v1.saved_model.build_signature_def(
        inputs={
            "string_input": string_input,
            "float_input": float_input,
            "int_input": int_input
        },
        outputs={
            "string_output": string_output,
            "float_output": float_output,
            "int_output": int_output
        },
        method_name=tf.compat.v1.saved_model.signature_constants.PREDICT_METHOD_NAME
    )

    builder.add_meta_graph_and_variables(
      session, [tf.compat.v1.saved_model.tag_constants.SERVING],
      signature_def_map={
          'inputs': prediction_signature,
          tf.compat.v1.saved_model.signature_constants
          .DEFAULT_SERVING_SIGNATURE_DEF_KEY: prediction_signature,
      },
      main_op=tf.compat.v1.tables_initializer(),
      strip_default_attrs=True)

    builder.save()


if __name__ == "__main__":
    main()
