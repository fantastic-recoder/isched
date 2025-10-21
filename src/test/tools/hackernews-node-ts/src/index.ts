import { execute, parse } from "graphql";
import { schema } from "./schema.js";

async function main() {
    const myQuery = parse(`query { info }`);

    const result = await execute({
        schema,
        document: myQuery,
    });

    console.log(result);
}

main();
