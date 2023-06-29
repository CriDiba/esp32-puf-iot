const fs = require("fs");

const roleArn = "arn:aws:iam::880663201134:role/JITPRole";

const policy = {
  Version: "2012-10-17",
  Statement: [
    {
      Effect: "Allow",
      Action: ["iot:Connect"],
      Resource: [
        "arn:aws:iot:eu-west-1:880663201134:client/${iot:Connection.Thing.ThingName}",
      ],
    },
    {
      Effect: "Allow",
      Action: ["iot:Publish", "iot:Receive"],
      Resource: ["arn:aws:iot:eu-west-1:880663201134:topic/*"],
    },
    {
      Effect: "Allow",
      Action: ["iot:Subscribe"],
      Resource: ["arn:aws:iot:eu-west-1:880663201134:topicfilter/*"],
    },
  ],
};

const templateBody = {
  Parameters: {
    "AWS::IoT::Certificate::CommonName": { Type: "String" },
    "AWS::IoT::Certificate::Country": { Type: "String" },
    "AWS::IoT::Certificate::Id": { Type: "String" },
  },
  Resources: {
    thing: {
      Type: "AWS::IoT::Thing",
      Properties: {
        ThingName: { Ref: "AWS::IoT::Certificate::CommonName" },
        AttributePayload: {
          version: "v1",
          country: { Ref: "AWS::IoT::Certificate::Country" },
        },
      },
    },
    certificate: {
      Type: "AWS::IoT::Certificate",
      Properties: {
        CertificateId: { Ref: "AWS::IoT::Certificate::Id" },
        Status: "ACTIVE",
      },
    },
    policy: {
      Type: "AWS::IoT::Policy",
      Properties: {
        PolicyDocument: JSON.stringify(policy),
      },
    },
  },
};

const provisioningModel = {
  templateBody: JSON.stringify(templateBody),
  roleArn,
};

try {
  const content = JSON.stringify(provisioningModel);
  fs.writeFileSync("jitp_template.json", content);
} catch (err) {
  console.error(err);
}
